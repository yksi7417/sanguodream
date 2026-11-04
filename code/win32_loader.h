#ifndef WIN32_LOADER_H
#define WIN32_LOADER_H
/* The load of the game on worker threads, so that the window does not
   freeze. Without it, the main thread reads and decodes the models for
   approximately 13 s, and the user cannot move the window.

   The load is a list of jobs:
   - the splash art, first, so that the picture shows early,
   - the fonts: the files and the build of the ImGui font atlas,
   - each job of the files of the renderer (d3d11_asset_prepare).
   The workers take the jobs in order. They read and decode the files into
   a slot, a buffer with its own arena. The main thread pumps the messages,
   draws the splash (win32_splash.h) and makes the D3D11 resources of the
   ready slots (win32_loader_upload). Then it gives the slot back.

   Rules for the threads:
   - A worker makes no D3D11 call. The device context is not thread safe.
   - While the workers run, the main thread makes no ImGui call. The font
     job uses the ImGui context and its allocator. win32_window.imgui_paused
     keeps the input of the window away from ImGui. */

#include "win32_window.h"
#include "ui/ui_fonts.h"

#define WIN32_LOADER_MAX_WORKERS   4
#define WIN32_LOADER_SLOTS         (WIN32_LOADER_MAX_WORKERS + 2)
#define WIN32_LOADER_SLOT_SIZE     Megabytes(48)   /* materials.jpg needs approximately 40 MB, the largest model approximately 13 MB */
#define WIN32_LOADER_UPLOAD_MS     8.0             /* the time of the uploads in one frame */

#define WIN32_LOAD_JOB_SPLASH      0
#define WIN32_LOAD_JOB_FONTS       1
#define WIN32_LOAD_JOB_FIRST_ASSET 2
#define WIN32_LOAD_JOB_COUNT       (WIN32_LOAD_JOB_FIRST_ASSET + D3D11_ASSET_COUNT)

#define WIN32_LOAD_SLOT_FREE  0
#define WIN32_LOAD_SLOT_BUSY  1
#define WIN32_LOAD_SLOT_READY 2

struct win32_load_slot
{
    volatile LONG    state;
    u32              job;
    memory_arena     arena;
    d3d11_asset_data data;    /* the splash job puts its picture in data.picture */
};

struct win32_loader
{
    d3d11_renderer  *renderer;
    memory_arena    *renderer_scratch;   /* the main thread: the placeholders of the uploads */
    const char      *font_data_dir;
    u32              font_pixels;
    ui_font_result   fonts;              /* the font job writes it */

    volatile LONG    next_job;
    volatile LONG    stop;
    HANDLE           free_slots;         /* a semaphore: its count is the number of free slots */
    HANDLE           workers[WIN32_LOADER_MAX_WORKERS];
    u32              worker_count;
    win32_load_slot  slots[WIN32_LOADER_SLOTS];
    u8              *slot_memory;
    u32              done_jobs;          /* the main thread: the jobs that it took from the slots */

    ID3D11ShaderResourceView *splash_view;
    f32                       splash_aspect;
};

internal void
win32_loader_prepare(win32_loader *loader, win32_load_slot *slot)
{
    if (slot->job == WIN32_LOAD_JOB_SPLASH)
    {
        slot->data.picture = d3d11_read_png_file(loader->renderer, HEXDOM_SPLASH_FILE, &slot->arena);
    }
    else if (slot->job == WIN32_LOAD_JOB_FONTS)
    {
        /* The DX11 backend makes the texture of the atlas in the first frame after the load. */
        loader->fonts = ui_fonts_load(loader->renderer->read_file, loader->font_data_dir, HEXDOM_CONTENT_PACK, loader->font_pixels);
        ImGui::GetIO().Fonts->Build();
    }
    else
    {
        d3d11_asset_prepare(loader->renderer, slot->job - WIN32_LOAD_JOB_FIRST_ASSET, &slot->arena, &slot->data);
    }
}

internal DWORD WINAPI
win32_loader_worker(LPVOID parameter)
{
    win32_loader *loader = (win32_loader *)parameter;
    while (!loader->stop)
    {
        /* Wait for a free slot, but look at the stop flag from time to time. */
        if (WaitForSingleObject(loader->free_slots, 50) != WAIT_OBJECT_0) continue;
        LONG job = InterlockedIncrement(&loader->next_job) - 1;
        if (job >= WIN32_LOAD_JOB_COUNT || loader->stop)
        {
            ReleaseSemaphore(loader->free_slots, 1, 0);
            break;
        }
        win32_load_slot *slot = 0;
        for (u32 index = 0; !slot; index = (index + 1) % WIN32_LOADER_SLOTS)
        {
            if (InterlockedCompareExchange(&loader->slots[index].state, WIN32_LOAD_SLOT_BUSY, WIN32_LOAD_SLOT_FREE) == WIN32_LOAD_SLOT_FREE)
            {
                slot = loader->slots + index;
            }
        }
        slot->job = (u32)job;
        slot->arena.used = 0;
        slot->data = {};
        win32_loader_prepare(loader, slot);
        InterlockedExchange(&slot->state, WIN32_LOAD_SLOT_READY);   /* a full barrier: the main thread sees the data */
    }
    return 0;
}

/* Starts the workers. renderer must have had d3d11_renderer_start. False
   if the memory or a thread is not available: then the caller loads on
   the main thread. */
internal b32
win32_loader_start(win32_loader *loader, d3d11_renderer *renderer, memory_arena *renderer_scratch,
                   const char *font_data_dir, u32 font_pixels)
{
    *loader = {};
    loader->renderer         = renderer;
    loader->renderer_scratch = renderer_scratch;
    loader->font_data_dir    = font_data_dir;
    loader->font_pixels      = font_pixels;
    loader->slot_memory = (u8 *)VirtualAlloc(0, (SIZE_T)(WIN32_LOADER_SLOT_SIZE * WIN32_LOADER_SLOTS), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!loader->slot_memory) return false;
    for (u32 index = 0; index < WIN32_LOADER_SLOTS; ++index)
    {
        arena_init(&loader->slots[index].arena, loader->slot_memory + WIN32_LOADER_SLOT_SIZE * index, WIN32_LOADER_SLOT_SIZE);
    }
    loader->free_slots = CreateSemaphoreA(0, WIN32_LOADER_SLOTS, WIN32_LOADER_SLOTS, 0);

    /* One core stays for the main thread. */
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    u32 wanted = system.dwNumberOfProcessors > 1 ? system.dwNumberOfProcessors - 1 : 1;
    wanted = Minimum(wanted, (u32)WIN32_LOADER_MAX_WORKERS);
    for (u32 index = 0; index < wanted && loader->free_slots; ++index)
    {
        HANDLE worker = CreateThread(0, 0, win32_loader_worker, loader, 0, 0);
        if (!worker) break;
        loader->workers[loader->worker_count++] = worker;
    }
    return loader->worker_count > 0;
}

/* The fraction of the jobs that the main thread has finished, from 0 to 1. */
internal f32
win32_loader_progress(win32_loader *loader)
{
    return (f32)loader->done_jobs / (f32)WIN32_LOAD_JOB_COUNT;
}

internal b32
win32_loader_done(win32_loader *loader)
{
    return loader->done_jobs >= WIN32_LOAD_JOB_COUNT;
}

/* The main thread: makes the D3D11 resources of the ready slots until the
   time of the frame for the uploads is over. */
internal void
win32_loader_upload(win32_loader *loader)
{
    LARGE_INTEGER frequency, start, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    for (u32 index = 0; index < WIN32_LOADER_SLOTS; ++index)
    {
        win32_load_slot *slot = loader->slots + index;
        if (InterlockedCompareExchange(&slot->state, WIN32_LOAD_SLOT_READY, WIN32_LOAD_SLOT_READY) != WIN32_LOAD_SLOT_READY) continue;
        if (slot->job == WIN32_LOAD_JOB_SPLASH)
        {
            const png_image *picture = &slot->data.picture;
            loader->splash_view = d3d11_make_picture_texture(loader->renderer, picture, false);
            if (loader->splash_view) loader->splash_aspect = (f32)picture->height / (f32)picture->width;
        }
        else if (slot->job != WIN32_LOAD_JOB_FONTS)
        {
            d3d11_asset_upload(loader->renderer, slot->job - WIN32_LOAD_JOB_FIRST_ASSET, &slot->data, loader->renderer_scratch);
        }
        loader->done_jobs++;
        InterlockedExchange(&slot->state, WIN32_LOAD_SLOT_FREE);
        ReleaseSemaphore(loader->free_slots, 1, 0);
        QueryPerformanceCounter(&now);
        if ((f64)(now.QuadPart - start.QuadPart) * 1000.0 / (f64)frequency.QuadPart > WIN32_LOADER_UPLOAD_MS) break;
    }
}

/* Waits for the workers and frees the slots. After a stop before the end,
   the workers finish their current job and then stop. */
internal void
win32_loader_finish(win32_loader *loader, b32 stop)
{
    if (stop) InterlockedExchange(&loader->stop, 1);
    if (loader->worker_count) WaitForMultipleObjects(loader->worker_count, loader->workers, TRUE, INFINITE);
    for (u32 index = 0; index < loader->worker_count; ++index) CloseHandle(loader->workers[index]);
    loader->worker_count = 0;
    if (loader->free_slots) CloseHandle(loader->free_slots);
    loader->free_slots = 0;
    if (loader->slot_memory) VirtualFree(loader->slot_memory, 0, MEM_RELEASE);
    loader->slot_memory = 0;
}

#endif
