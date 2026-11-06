FROM gcc:14-bookworm

WORKDIR /app

COPY server.c .
COPY elements.h .
COPY public ./public

RUN gcc server.c -o server

ENV PORT=10000

EXPOSE 10000

CMD ["./server"]
