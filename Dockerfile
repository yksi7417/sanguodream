# The fly.io site of the web game (docs/WEB.md, "The fly.io site").
# deploy_web.bat builds build/web on Windows first. This image only serves
# those files behind a password. It does not compile the game.
FROM python:3.12-slim

RUN useradd --system --no-create-home hexdom
WORKDIR /srv
COPY web/server.py /srv/server.py
COPY build/web/index.html build/web/index.js build/web/index.wasm build/web/index.data /srv/web/
# The server sends FILE.gz to a browser that accepts gzip.
RUN gzip --keep -9 /srv/web/index.html /srv/web/index.js /srv/web/index.wasm /srv/web/index.data

USER hexdom
EXPOSE 8080
CMD ["python", "-u", "/srv/server.py", "/srv/web"]
