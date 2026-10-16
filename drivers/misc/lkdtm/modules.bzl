load(
    "@kleaf//build/kernel/kleaf:hermetic_tools.bzl",
    "hermetic_genrule",
)
load(
    "@kleaf//build/kernel/kleaf:kernel.bzl",
    "ddk_library",
    "ddk_prebuilt_object",
)

def register_modules(registry):
    registry.register(
        name = "drivers/misc/lkdtm/lkdtm",
        out = "lkdtm.ko",
        config = "CONFIG_LKDTM",
        srcs = [
            # do not sort
            "drivers/misc/lkdtm/core.c",
            "drivers/misc/lkdtm/bugs.c",
            "drivers/misc/lkdtm/heap.c",
            "drivers/misc/lkdtm/perms.c",
            "drivers/misc/lkdtm/refcount.c",
            "drivers/misc/lkdtm/usercopy.c",
            "drivers/misc/lkdtm/stackleak.c",
            "drivers/misc/lkdtm/cfi.c",
            "drivers/misc/lkdtm/fortify.c",
            "drivers/misc/lkdtm/lkdtm.h",
        ],
        conditional_srcs = {
            "CONFIG_PPC_64S_HASH_MMU": {
                True: [
                    # do not sort
                    "drivers/misc/lkdtm/powerpc.c",
                    "drivers/misc/lkdtm/lkdtm.h",
                ],
            },
        },
        hook_deps = ["librodata_objcopy_wrapped"],
    )

    def define_lkdtm_deps(target_variant):
        ddk_library(
            name = "{}_librodata".format(target_variant),
            kernel_build = "{}_base_kernel".format(target_variant),
            hdrs = ["drivers/misc/lkdtm/lkdtm.h"],
            srcs = ["drivers/misc/lkdtm/rodata.c"],
            deps = [
                ":additional_msm_headers",
                "//common:all_headers",
            ],
        )

        hermetic_genrule(
            name = "{}_librodata_objcopy".format(target_variant),
            srcs = [":{}_librodata".format(target_variant)],
            outs = [
                "{}_librodata_objcopy/.rodata.o.cmd_shipped".format(target_variant),
                "{}_librodata_objcopy/rodata.o_shipped".format(target_variant),
            ],
            cmd = """
                for filename in $(execpaths :{target_variant}_librodata); do
                    case "$$filename" in
                        *.o_shipped)
                            llvm-objcopy --rename-section .noinstr.text=.rodata,alloc,readonly,load,contents \\
                                $$filename $(RULEDIR)/{target_variant}_librodata_objcopy/$$(basename $$filename)
                        ;;
                        *)
                            cp $$filename $(RULEDIR)/{target_variant}_librodata_objcopy/$$(basename $$filename)
                        ;;
                    esac
                done
            """.format(target_variant = target_variant),
            use_cc_toolchain = True,
        )

        ddk_prebuilt_object(
            name = "{}_librodata_objcopy_wrapped".format(target_variant),
            src = "{}_librodata_objcopy/rodata.o_shipped".format(target_variant),
            cmd = "{}_librodata_objcopy/.rodata.o.cmd_shipped".format(target_variant),
        )

    registry.register_hook(struct(func = define_lkdtm_deps))
