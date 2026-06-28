FROM eclipse-mosquitto AS build-base

RUN apk --no-cache add bash make gcc libc-dev mosquitto-dev tzdata ncurses-dev

RUN mkdir -p /build
COPY ./ /build
WORKDIR /build
RUN make

FROM eclipse-mosquitto
RUN apk --no-cache add libstdc++ mosquitto-libs ncurses-dev tzdata
COPY --from=build-base /build/submqtt /usr/bin

USER nobody
CMD ["submqtt"]
