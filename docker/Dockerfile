FROM ubuntu:20.04
ENV TZ=Asia/Singapore
ENV DEBIAN_FRONTEND=noninteractive 
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN cd /etc/apt && sed -i 's/archive.ubuntu.com/ftp.ina.kaist.ac.kr/g' sources.list
RUN cd /
RUN apt-get update -qq && apt-get install -y --no-install-recommends python3 libgtk-3-0
RUN mkdir -p /ns-3.19/
COPY build /ns-3.19/build
COPY config /ns-3.19/config
COPY workloads /ns-3.19/workloads
COPY mix /ns-3.19/mix
COPY main.py /ns-3.19/main.py
COPY template.config /ns-3.19/template.config
WORKDIR /ns-3.19
ENTRYPOINT ["./main.py"]