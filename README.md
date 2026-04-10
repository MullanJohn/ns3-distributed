# ns-3 Continuum

An implementation of the computing continuum in ns-3. This project aims to enable simulating applications with remote processing workloads (e.g., AI-based applications such as LLM, image processing, ...etc).


## Build Commands

```bash
./ns3 configure --enable-examples --enable-tests  # Configure
./ns3 build                                        # Build
./test.py -s continuum                             # Run module tests
./ns3 run continuum-example                        # Run example
```
