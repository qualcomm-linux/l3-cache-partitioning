# Contributing

## Branching Strategy

Work on branches based off `main`, with PRs targeting `main`.

## Submitting a Pull Request

1. Fork and clone the repository.
2. Create a new branch from `main`.
3. Make changes without breaking existing functionality.
4. Sign commits using the DCO:
   ```
   git commit -s -m "Your commit message"
   ```
5. Rebase against upstream before pushing.
6. Open a PR against `main`.

## Coding Style

Follow the [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html).

## Build and Test

```sh
make KDIR=<path-to-kernel> ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

See [README.md](README.md) for full build and test instructions.

## Security

To report a security vulnerability, see [SECURITY.md](SECURITY.md).
