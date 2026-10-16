# SPDX-License-Identifier: GPL-2.0-only

load("//build/kernel/kleaf:kernel.bzl", "ddk_uapi_headers")
load(":soc_repo_path.bzl", "SOC_MODULES_REPO_PATH")

_TECHPACK_UAPI_HEADERS = {
    # Audio consumers include these headers without the audio/ prefix.
    "audio": ("audio-kernel:audio_uapi_headers", "include/uapi/audio"),
    "display": ("display-drivers:uapi_headers", "include/uapi"),
    "ipa": ("dataipa:ipa_uapi_headers", "include/uapi"),
    "smmu_proxy": ("securemsm-kernel:smmu_proxy_uapi_headers", "include/uapi"),
}

def define_techpack_uapi_headers(stem, kernel_build):
    """Create sanitized techpack UAPI archives for a kernel variant.

    Args:
        stem: Target and variant prefix for generated rules.
        kernel_build: Kernel build providing the header sanitization tools.

    Returns:
        Labels of archives containing the headers under usr/include.
    """
    archives = []
    for name, (src, strip_prefix) in _TECHPACK_UAPI_HEADERS.items():
        target = "{}_{}_uapi_headers".format(stem, name)
        ddk_uapi_headers(
            name = target,
            srcs = ["//{}/qcom/opensource/{}".format(SOC_MODULES_REPO_PATH, src)],
            out = "{}-uapi-headers.tar.gz".format(name),
            kernel_build = kernel_build,
            strip_prefix = strip_prefix,
        )
        archives.append(":" + target)
    return archives
