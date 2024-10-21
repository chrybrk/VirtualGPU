#define IMPLEMENT_BUILD_C

#include "build.h"

#define CC "gcc"
#define CFALGS "-O2", "-g0", "-static"

const char *files[] = {
	"kbd",
	"test"
};

void create_kernel_essentials(const char *path, const char *rootfs_out, const char *initramfs_out);

int main(int argc, char **argv)
{
	create_directories("bin out shared");

	size_t len = sizeof(files) / sizeof(files[0]);

	for (size_t i = 0; i < len; ++i)
	{
		if (needs_recompilation(writef("shared/%s", files[i]), (const char*[]){ writef("src/%s.c", files[i]) }, 1))
			CMD(CC, CFALGS, writef("src/%s.c", files[i]), "-o", writef("shared/%s", files[i]));
	}

	if (needs_recompilation(writef("shared/card"), (const char*[]){ "src/card.c" }, 1))
		CMD(CC, CFALGS, "-I/usr/include/libdrm/", "src/card.c", "-o", "shared/card", "-ldrm");

	char *test = run_command("test -f out/rootfs.ext4 && echo 0 || echo 1");
	if (test[0] == '1')
		create_kernel_essentials("rootfs", "out/rootfs.ext4", "out/initramfs.cpio");

	return 0;
}

void create_kernel_essentials(const char *path, const char *rootfs_out, const char *initramfs_out)
{
	if (path == NULL || rootfs_out == NULL || initramfs_out == NULL) ERROR("[!] PASSING NULL TO ARGUMENT CAN BE DANGEROUS.");

	CMD("mkdir", "-p", path);
	chdir(path);

	const char *basic_linux_dirs[] = {
		"bin",
		"sbin",
		"etc",
		"proc",
		"sys",
		"dev",
		"tmp",
		"var",
		"usr",
		"dev",
		"mnt",
		"var/lib",
		"var/run",
		"usr/bin",
		"usr/sbin"
	};

	size_t basic_linux_dirs_len = sizeof(basic_linux_dirs) / sizeof(basic_linux_dirs[0]);
	for (size_t i = 0; i < basic_linux_dirs_len; ++i)
		CMD("mkdir", "-p", basic_linux_dirs[i]);

	CMD("sudo", "mknod", "-m", "666", "dev/null", "c", "1", "3");
	CMD("sudo", "mknod", "-m", "666", "dev/zero", "c", "1", "5");
	CMD("sudo", "mknod", "-m", "622", "dev/console", "c", "5", "1");

	char *busybox_path = run_command("which busybox");
	size_t busybox_path_len = strlen(busybox_path);

	if (busybox_path_len <= 0)
	{
		free(busybox_path);
		ERROR("No busybox found, exiting.");
	}

	CMD("cp", substr(busybox_path, 0, strlen(busybox_path) - 1), "bin/");

	char *busybox_items = run_command("busybox --list");
	size_t n; char **busybox_item_list = seperate('\n', busybox_items, &n);

	for (size_t i = 0; i < n; ++i)
		if (!strcmp(busybox_item_list[i], "busybox")) continue;
		else CMD("ln", "-s", "busybox", writef("bin/%s", busybox_item_list[i]));

	CMD("cp ../script/init .");
	CMD("chmod", "+x", "init");
	chdir("..");

	CMD("dd", "if=/dev/zero", writef("of=%s", rootfs_out), "bs=1M", "count=64");
	CMD("mkfs.ext4", "-F", rootfs_out);

	CMD("mkdir", "-p", "mnt");
	CMD("sudo", "mount", "-o", "loop", rootfs_out, "mnt");
	CMD("sudo", "cp", "-r", writef("%s/*", path), "mnt");
	CMD("sudo", "umount", "mnt");
	CMD("rmdir", "mnt");

	chdir(path);
	CMD("find", ".", "|", "cpio", "-H", "newc", "-o", ">", writef("../%s", initramfs_out));
	chdir("..");

	CMD("rm -r", path);

	INFO("`%s` and `%s` have been created.", rootfs_out, initramfs_out);
}
