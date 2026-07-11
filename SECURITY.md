# Security Policy

## Reporting a vulnerability

Please report suspected security vulnerabilities **privately** — do not open a public
issue for them.

- Preferred: use GitHub's private vulnerability reporting via
  [**Report a vulnerability**](https://github.com/ideocentric/partialfkr/security/advisories/new).
  (This requires *Private vulnerability reporting* to be enabled for the repository under
  Settings → Security.)
- Alternatively, email **oss@ideocentric.com** with the details.

Please include:

- the affected version (see **Help → About**, or the release tag),
- your platform (macOS / Windows / Linux and architecture),
- a description of the issue and its impact, and
- steps to reproduce or a proof of concept, if you have one.

You can expect an initial acknowledgement within a few days. Because PartialFKR is a small
open-source project maintained on a best-effort basis, please allow reasonable time for a
fix before any public disclosure.

## Supported versions

Only the **latest released version** receives security fixes. Older versions are not
maintained; please update to the current release before reporting.

| Version | Supported |
| ------- | --------- |
| Latest release | ✅ |
| Older releases | ❌ |

## Scope

PartialFKR is an offline desktop application. It performs no network communication of its
own (no telemetry, analytics, or update checks — see the
[privacy policy](https://partialfkr.org/privacy.html)), so the relevant attack surface is
primarily **local**: parsing of untrusted input files (audio files and `.pfkr` project
documents) and the handling of exported files. Reports about crashes, memory-safety issues,
or unexpected code execution triggered by malformed input files are especially welcome.

## How releases are distributed and signed

Only binaries obtained from the official
[GitHub Releases](https://github.com/ideocentric/partialfkr/releases) page are supported.
Releases are built automatically by GitHub Actions from this repository; macOS builds are
signed and notarised, and Windows builds are Authenticode-signed via the
[SignPath Foundation](https://signpath.org). See the
[security & code-signing page](https://partialfkr.org/security.html) for details on how
builds are produced and how to verify them.
