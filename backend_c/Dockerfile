FROM gcc:latest

WORKDIR /app

COPY . .

RUN gcc aeroqueue.c -o aeroqueue_server

CMD ["./aeroqueue_server"]