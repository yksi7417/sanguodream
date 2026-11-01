# The fly.io site of the web game (docs/WEB.md, "The fly.io site").
# deploy_web.bat builds build/web on Windows first. This image serves those
# files to the members, keeps the saves and relays the optional
# YouTube broadcast. It does not compile the game.
FROM python:3.12-slim

# ffmpeg is for the YouTube broadcast (web/broadcast.py). node runs the referee
# of the shared games (web/referee.py; docs/MULTIPLAYER.md, MP6).
RUN apt-get update \
 && apt-get install --yes --no-install-recommends ffmpeg nodejs \
 && rm -rf /var/lib/apt/lists/*
RUN useradd --system --no-create-home hexdom
WORKDIR /srv
COPY web/requirements.txt /srv/requirements.txt
RUN pip install --no-cache-dir --requirement /srv/requirements.txt
COPY web/server.py web/http_common.py web/games_api.py web/auth.py web/email_link.py web/storage.py web/pages.py web/broadcast.py \
     web/game_files.py web/rooms.py web/room_state.py web/room_socket.py web/referee.py /srv/
# The referee reads the rules and the maps of the packs from ../data (.dockerignore keeps out the models).
COPY build/referee.js build/referee.wasm /srv/referee/
COPY data /srv/data
COPY build/web/index.html build/web/index.js build/web/index.wasm build/web/index.data build/web/shared_game.js /srv/web/
# The server sends FILE.gz to a browser that accepts gzip.
RUN gzip --keep -9 /srv/web/index.html /srv/web/index.js /srv/web/index.wasm /srv/web/index.data /srv/web/shared_game.js

USER hexdom
EXPOSE 8080
CMD ["python", "-u", "/srv/server.py", "/srv/web"]
