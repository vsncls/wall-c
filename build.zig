const std = @import("std");

fn addCSources(
    b: *std.Build,
    exe: *std.Build.Step.Compile,
    sources: []const []const u8,
    flags: []const []const u8,
) void {
    for (sources) |src| {
        exe.addCSourceFile(.{ .file = b.path(src), .flags = flags });
    }
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const c_flags = &[_][]const u8{
        "-std=c99",
        "-D_POSIX_C_SOURCE=200809L",
        "-Isrc",
    };
    const test_flags = &[_][]const u8{
        "-std=c99",
        "-D_POSIX_C_SOURCE=200809L",
        "-Isrc",
        "-DWALL_TEST",
    };
    const sanitize_test_flags = &[_][]const u8{
        "-std=c99",
        "-D_POSIX_C_SOURCE=200809L",
        "-Isrc",
        "-DWALL_TEST",
    };

    const app_sources = &[_][]const u8{
        "src/main.c",
        "src/config.c",
        "src/validate.c",
        "src/packet.c",
        "src/net.c",
        "src/cli.c",
    };
    const test_sources = &[_][]const u8{
        "tests/test.c",
        "src/config.c",
        "src/validate.c",
        "src/packet.c",
        "src/net.c",
    };

    const exe = b.addExecutable(.{
        .name = "wall-c",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    addCSources(b, exe, app_sources, c_flags);
    exe.linkLibC();
    b.installArtifact(exe);

    const test_exe = b.addExecutable(.{
        .name = "wall-c-test",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    addCSources(b, test_exe, test_sources, test_flags);
    test_exe.linkLibC();
    b.installArtifact(test_exe);

    const run_tests = b.addRunArtifact(test_exe);
    const test_step = b.step("test", "Build and run unit tests");
    test_step.dependOn(&run_tests.step);

    const sanitize_exe = b.addExecutable(.{
        .name = "wall-c-test-sanitize",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .sanitize_c = .full,
        }),
    });
    addCSources(b, sanitize_exe, test_sources, sanitize_test_flags);
    sanitize_exe.linkLibC();
    b.installArtifact(sanitize_exe);

    const run_sanitize = b.addRunArtifact(sanitize_exe);
    const sanitize_step = b.step("sanitize", "Build and run sanitizer tests");
    sanitize_step.dependOn(&run_sanitize.step);

    const release_exe = b.addExecutable(.{
        .name = "wall-c-release",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = .ReleaseFast,
            .link_libc = true,
        }),
    });
    addCSources(b, release_exe, app_sources, c_flags);
    release_exe.linkLibC();
    b.installArtifact(release_exe);

    const release_step = b.step("release", "Build release binary");
    release_step.dependOn(&release_exe.step);
}
