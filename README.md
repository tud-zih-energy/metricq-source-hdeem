# metricq-source-hdeem

## Build docker image

1. Place the `hdeem` and `hdeem-devel` rpm packages in the source directory
2. Run `docker build -t metricq-source-hdeem .` in the source directory

The built docker image `metricq-source-hdeem` behaves like the `metricq-source-hdeem` executable.
This means, you can run it with parameters. For instance, this will print the help message:

`docker run --rm metricq-source-hdeem --help`
