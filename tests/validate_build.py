import argparse
import struct
from pathlib import Path

import yaml
from elftools.elf.elffile import ELFFile
from intelhex import IntelHex


LAYOUTS = {
    "nrf5340_audio_dk": {
        "starts": {
            "dbgtest": 0,
            "app_a": 0x20000,
            "app_b": 0x90000,
            "app_c": 0x1000000,
            "app_d": 0x1020000,
        },
        "retained": 0x20070000,
    },
    "nrf54l15dk": {
        "starts": {
            "dbgtest": 0,
            "app_a": 0x20000,
            "app_b": 0xB0000,
            "app_c": 0x140000,
            "app_d": 0x160000,
        },
        "retained": 0x2002E000,
    },
}


def read_config(path):
    return dict(
        line.strip().split("=", 1)
        for line in path.read_text().splitlines()
        if line.startswith("CONFIG_") and "=" in line
    )


def validate_build(build_dir):
    domains = yaml.safe_load((build_dir / "domains.yaml").read_text())
    boot_config = read_config(build_dir / "dbgtest" / "zephyr" / ".config")
    layout = LAYOUTS[boot_config["CONFIG_BOARD"].strip('"')]
    starts = layout["starts"]
    retained = layout["retained"]
    assert {domain["name"] for domain in domains["domains"]} == set(starts)
    combined = IntelHex()

    for domain in domains["domains"]:
        name = domain["name"]
        folder = Path(domain["build_dir"]) / "zephyr"
        config = read_config(folder / ".config")
        image = IntelHex(str(folder / "zephyr.hex"))
        assert image.minaddr() == starts[name], (name, hex(image.minaddr()))
        image.start_addr = None
        combined.merge(image, overlap="error")

        with (folder / "zephyr.elf").open("rb") as stream:
            elf = ELFFile(stream)
            symbols = {
                symbol.name: symbol for symbol in elf.get_section_by_name(".symtab").iter_symbols()
            }
            assert elf.get_section_by_name(".debug_info") is not None
            if name != "dbgtest":
                for symbol in (
                    "debugger_globals",
                    "debugger_image_name",
                    "debugger_leaf",
                    "debugger_nested",
                    "_k_thread_obj_producer_thread",
                    "_k_thread_obj_consumer_thread",
                    "_k_thread_obj_observer_thread",
                    "_k_thread_stack_producer_thread",
                    "_k_thread_stack_consumer_thread",
                    "_k_thread_stack_observer_thread",
                ):
                    assert symbol in symbols, (name, symbol)
                for setting in (
                    "CONFIG_DEBUG_THREAD_INFO",
                    "CONFIG_THREAD_NAME",
                    "CONFIG_INIT_STACKS",
                    "CONFIG_NO_OPTIMIZATIONS",
                ):
                    assert config[setting] == "y", (name, setting)
                global_symbol = symbols["debugger_globals"]
                section = elf.get_section(global_symbol["st_shndx"])
                offset = global_symbol["st_value"] - section["sh_addr"]
                image_id = struct.unpack_from("<I", section.data(), offset + 4)[0]
                assert image_id == {"app_a": 1, "app_b": 2, "app_c": 3, "app_d": 4}[name]

            for segment in elf.iter_segments():
                if segment["p_type"] == "PT_LOAD":
                    start = segment["p_vaddr"]
                    end = start + segment["p_memsz"]
                    assert end <= retained or start >= retained + 0x1000, (name, "retained overlap")

            if elf["e_machine"] == "EM_ARM":
                stack, entry = struct.unpack(
                    "<II", bytes(image.tobinarray(start=starts[name], size=8))
                )
                ram_start = int(config["CONFIG_SRAM_BASE_ADDRESS"], 0)
                ram_end = ram_start + int(config["CONFIG_SRAM_SIZE"]) * 1024
                assert stack % 8 == 0 and ram_start < stack <= ram_end, (name, hex(stack))
                assert entry & 1 and starts[name] <= (entry & ~1) <= image.maxaddr()
                assert config["CONFIG_INIT_ARCH_HW_AT_BOOT"] == "y"
            else:
                assert elf["e_machine"] == "EM_RISCV"
                assert elf["e_entry"] == starts[name], (name, hex(elf["e_entry"]))

        print(f"{name}: start={starts[name]:#010x}, bytes={len(image)}, checks OK")

    print(f"{build_dir.name}: five domains; HEX disjoint; retained SRAM excluded; identities OK")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dirs", type=Path, nargs="+")
    args = parser.parse_args()
    for build_dir in args.build_dirs:
        validate_build(build_dir.resolve())


if __name__ == "__main__":
    main()