# Third-party notices

## gdsolver

PathWeaver is based on [gdsolver v0.6.1](https://github.com/gdsolver/gdsolver/tree/d09300a6f4c7946f26015fc86a6cb33e8c891a7a). The gdsolver authors' copyright notice is in the [MIT license](LICENSE). See the [fork acknowledgment](docs/UPSTREAM_PERMISSION.md).

## Geode SDK

[Geode SDK 5.11.2](https://github.com/geode-sdk/geode/tree/v5.11.2) uses the [Boost Software License 1.0](https://github.com/geode-sdk/geode/blob/v5.11.2/LICENSE.txt).

## GDReplayFormat

PathWeaver uses [libGDR](https://github.com/maxnut/GDReplayFormat/tree/d8d2f0b1193a46cdf47b413dc832616dd052b6a9) for GDR2 following the upstream Geode integration instructions. The pinned source tree contains no named license. [License clarification](https://github.com/maxnut/GDReplayFormat/issues/11).

## Eclipse API 1.9.4

The [Eclipse Team](https://eclipse.menu) distributes `eclipse.eclipse-menu` 1.9.4 under the Eclipse Public License 2.0, as stated in the package's About page. PathWeaver uses the original `config.hpp` and `events.hpp` headers for optional timing integration.

These headers are available under [EPL-2.0](third_party/eclipse_api/LICENSE-EPL-2.0.txt). Find the source in [third_party/eclipse_api/include/eclipse](third_party/eclipse_api/include/eclipse) or in `resources/b1nt1n.pathweaver/` inside the `.geode` ZIP archive. The archive includes the EPL-2.0 license text in the same directory as the headers.
