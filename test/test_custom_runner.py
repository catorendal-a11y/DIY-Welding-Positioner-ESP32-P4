"""Use the upstream Unity 2.7.0 tag instead of the lagging registry package."""
from platformio.public import UnityTestRunner


class CustomTestRunner(UnityTestRunner):
    # The stock runner injects registry Unity after lib_deps and can replace it.
    # Override only that source; keep PlatformIO's Unity build/output handling.
    EXTRA_LIB_DEPS = ["https://github.com/ThrowTheSwitch/Unity.git#b6763fbd9cedfacaa89e2ad9fd00d615a234e355"]
