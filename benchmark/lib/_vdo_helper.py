from lib.pubilc_funs import run


class VDO:
    vdo_create_shell = "vdo create --name={} --device={} --vdoLogicalSize={} --force --activate=enabled " \
                       "--blockMapCacheSize {} --vdoLogicalThreads={} --vdoPhysicalThreads={} " \
                       "--vdoHashZoneThreads={} --vdoCpuThreads={} --vdoAckThreads={} --vdoBioThreads={} "\
                       "--deduplication=disabled --compression=enabled --writePolicy=async"
    vdo_modify_shell = "vdo modify -n {} --blockMapCacheSize {} --vdoLogicalThreads={} --vdoPhysicalThreads={} " \
                       "--vdoHashZoneThreads={} --vdoCpuThreads={} --vdoAckThreads={} --vdoBioThreads={} "

    def __init__(self, name, mount_point=None):
        self.name = name
        self.mount_point = mount_point

    def create(self, size, device='/dev/ram0', blockMapCacheSize=128, vdoLogicalThreads=2, vdoPhysicalThreads=2, vdoHashZoneThreads=1, vdoCpuThreads=1, vdoAckThreads=1, vdoBioThreads=1):
        # Fixed to usr ram disk, TODO: use any block device
        run("modprobe brd rd_nr=1 rd_size=40485760 max_part=0")
        run(self.vdo_create_shell.format(self.name, device, size, blockMapCacheSize, vdoLogicalThreads, vdoPhysicalThreads, vdoHashZoneThreads, vdoCpuThreads, vdoAckThreads, vdoBioThreads))

    def recreate(self, blockMapCacheSize, vdoLogicalThreads, vdoPhysicalThreads, vdoHashZoneThreads, vdoCpuThreads,
                 vdoAckThreads, vdoBioThreads):
        self.stop()
        self.modify((blockMapCacheSize, vdoLogicalThreads, vdoPhysicalThreads, vdoHashZoneThreads, vdoCpuThreads,
                     vdoAckThreads, vdoBioThreads))
        self.start()

    def remove(self):
        self.stop()
        run("vdo remove -n {name}".format(name=self.name))
        run("rmmod brd")

    def modify(self, package):
        run(self.vdo_modify_shell.format(self.name, *package))

    def start(self):
        run("vdo start -n {name}".format(name=self.name))
        if self.mount_point is not None:
            run("mkfs.xfs /dev/mapper/{name}".format(name=self.name))
            run("mount /dev/mapper/{name} {mount_point}".format(
                name=self.name, mount_point=self.mount_point))

    def stop(self):
        if self.mount_point is not None:
            run("rm -rf {mount_point}/fio".format(mount_point=self.mount_point))
            run("umount {mount_point}".format(mount_point=self.mount_point))
        run("vdo stop -n {name}".format(name=self.name))

# vdo = VDO('vdo-ram', '/ramdisk')
