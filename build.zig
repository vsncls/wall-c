const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "wall-c",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    exe.addCSourceFile(.{ .file = b.path("main.c"), .flags = &[_][]const u8{"-std=c99"} });
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
    test_exe.addCSourceFile(.{ .file = b.path("main.c"), .flags = &[_][]const u8{"-std=c99", "-DWALL_TEST"} });
    test_exe.addCSourceFile(.{ .file = b.path("test.c"), .flags = &[_][]const u8{"-std=c99", "-DWALL_TEST"} });
    test_exe.linkLibC();
    b.installArtifact(test_exe);

    const run_tests = b.addRunArtifact(test_exe);
    const test_step = b.step("test", "Build and run unit tests");
    test_step.dependOn(&run_tests.step);
}
