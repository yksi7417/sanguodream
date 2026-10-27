# The fly.io site of the web game (docs/WEB.md, "The fly.io site").
# deploy_web.bat builds build/web on Windows first. This image serves those
# files to the members of the house, keeps the saves and relays the optional
# YouTube broadcast. It does not compile the game.
FROM python:3.12-slim

# ffmpeg is for the YouTube broadcast (web/broadcast.py).
RUN apt-get update \
 && apt-get install --yes --no-install-recommends ffmpeg \
 && rm -rf /var/lib/apt/lists/*
RUN useradd --system --no-create-home hexdom
WORKDIR /srv
COPY web/requirements.txt /srv/requirements.txt
RUN pip install --no-cache-dir --requirement /srv/requirements.txt
COPY web/server.py web/auth.py web/storage.py web/pages.py web/broadcast.py /srv/
COPY build/web/index.html build/web/index.js build/web/index.wasm build/web/index.data /srv/web/
# The server sends FILE.gz to a browser that accepts gzip.
RUN gzip --keep -9 /srv/web/index.html /srv/web/index.js /srv/web/index.wasm /srv/web/index.data

USER hexdom
EXPOSE 8080
CMD ["python", "-u", "/srv/server.py", "/srv/web"]
