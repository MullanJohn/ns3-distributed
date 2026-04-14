# ns-3 Continuum

An implementation of the computing continuum in ns-3. This project aims to enable simulating applications with remote processing workloads (e.g., AI-based applications such as LLM, image processing, ...etc).


## Build Commands

```bash
./ns3 configure --enable-examples --enable-tests  # Configure
./ns3 build                                        # Build
./test.py -s continuum                             # Run module tests
./ns3 run continuum-example                        # Run example
```

## Development Dockerfile
```bash
git clone --branch main --single-branch https://github.com/MullanJohn/ns3-distributed.git
cd ns3-distributed
docker build -f Dockerfile.dev -t continuum-dev .
docker run -it -v "$(pwd)":/workspace/ns-3-dev/contrib/continuum continuum-dev
```

## Evaluation Dockerfile
```bash
git clone --branch evaluation --single-branch https://github.com/MullanJohn/ns3-distributed.git
cd ns3-distributed
docker build -f Dockerfile.eval -t continuum-eval .
docker run -it continuum-eval
```
