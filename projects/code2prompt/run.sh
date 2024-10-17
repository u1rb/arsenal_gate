docker build -t code2prompt-ubi8 .

docker run -it --rm code2prompt-ubi8 --help

docker create -it --name code2prompt-ubi8-temp code2prompt-ubi8

docker cp code2prompt-ubi8-temp:/root/.cargo/bin/code2prompt .

docker stop code2prompt-ubi8-temp

docker rm code2prompt-ubi8-temp

chmod +x code2prompt

./code2prompt --help