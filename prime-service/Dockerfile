FROM gcc:latest

WORKDIR /app

COPY src/ ./src/
RUN gcc src/main.c -o prime_service -lpthread

ENTRYPOINT ["./prime_service"]
