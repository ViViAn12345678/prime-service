FROM gcc:latest

WORKDIR /app

COPY main.c .

RUN gcc -pthread -o prime_service main.c

EXPOSE 9090

CMD ["./prime_service"]
