# How to run dockerised sim

1. Ensure in root dir of project
2. Run `docker build --build-arg ARCH=YOUR_ARCH -f ./scripts/runner.Dockerfile -t sc3-runner .` Replace YOUR_ARCH with either x86 or arm64 depending on system.
3. Run with `docker run -d -p 8080:8080 sc3-runner `

You can now connect the sc3 ui to localhost:8080 and it should work.
