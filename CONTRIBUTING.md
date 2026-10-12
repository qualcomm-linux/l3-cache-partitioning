# Contributing to l3-cache-partitioning

We welcome contributions to this project. Your help is essential for keeping
it great and for making it better.

## Branching Strategy

Contributors should develop on branches based off of `main` and pull requests
should be made against `main`.

## Submitting a Pull Request

1. Please read our [code of conduct](CODE-OF-CONDUCT.md) and [license](LICENSE.txt).
2. [Fork](https://github.com/qualcomm-linux/l3-cache-partitioning/fork) and clone the repository.

    ```bash
    git clone https://github.com/<username>/l3-cache-partitioning.git
    ```

3. Create a new branch based on `main`:

    ```bash
    git checkout -b <my-branch-name> main
    ```

4. Make your changes and ensure the module builds cleanly:

    ```bash
    make KDIR=/lib/modules/$(uname -r)/build W=1
    scripts/checkpatch.pl --strict <your-patch>
    ```

5. Commit with a DCO sign-off:

    ```bash
    git commit -s -m "cache_partition: describe your change"
    ```

6. Sync with upstream before pushing:

    ```bash
    git pull --rebase upstream main
    ```

7. Push and [open a pull request](https://github.com/qualcomm-linux/l3-cache-partitioning/pulls).

## Coding Style

Follow the Linux kernel coding style. Run `scripts/checkpatch.pl` before submitting.
Every source file must carry an SPDX-License-Identifier header and a Qualcomm
Innovation Center copyright notice.

## Security Analysis

All pull requests are automatically scanned with Semgrep. Resolve any flagged
findings before the PR can be merged.
