from lib.pubilc_funs import run
import os

class VDO:
    vdo_create_shell = "vdo create --name={} --device={} --vdoLogicalSize={} --force --activate=enabled " \
                       "--blockMapCacheSize={} --vdoLogicalThreads={} --vdoPhysicalThreads={} " \
                       "--vdoHashZoneThreads={} --vdoCpuThreads={} --vdoAckThreads={} --vdoBioThreads={} "\
                       "--deduplication={} --compression={} --writePolicy=async"
    vdo_modify_shell = "vdo modify -n {} --blockMapCacheSize={} --vdoLogicalThreads={} --vdoPhysicalThreads={} " \
                       "--vdoHashZoneThreads={} --vdoCpuThreads={} --vdoAckThreads={} --vdoBioThreads={} "

    def __init__(self, name, mount_point=None):
        self.name = name
        self.mount_point = mount_point

    def create(self, size, device='/dev/nvme0n1p1', blockMapCacheSize=128, vdoLogicalThreads=8, vdoPhysicalThreads=8, 
                vdoHashZoneThreads=4, vdoCpuThreads=16, vdoAckThreads=4, vdoBioThreads=4, 
                deduplication='enabled', compression='enabled'):
        # Fixed to usr ram disk, TODO: use any block device
        run("modprobe brd rd_nr=1 rd_size=40485760 max_part=0")
        vdo_shell = self.vdo_create_shell.format(self.name, device, size, blockMapCacheSize, vdoLogicalThreads, vdoPhysicalThreads,
            vdoHashZoneThreads, vdoCpuThreads, vdoAckThreads, vdoBioThreads, deduplication, compression)
        print(vdo_shell)
        run(vdo_shell)
        self.start()

    def recreate(self, blockMapCacheSize, vdoLogicalThreads, vdoPhysicalThreads, vdoHashZoneThreads, vdoCpuThreads,
                 vdoAckThreads, vdoBioThreads, deduplication, compression):
        self.stop()
        self.modify((blockMapCacheSize, vdoLogicalThreads, vdoPhysicalThreads, vdoHashZoneThreads, vdoCpuThreads,
                     vdoAckThreads, vdoBioThreads, deduplication, compression))
        self.start()

    def remove(self):
        self.stop()
        run("vdo remove -n {name}".format(name=self.name))
        run("rmmod brd")

    def modify(self, package):
        run(self.vdo_modify_shell.format(self.name, *package))

    def start(self):
        run("vdo start -n {name}".format(name=self.name))
        if not (self.mount_point is None):
            run("mkfs.xfs /dev/mapper/{name}".format(name=self.name))
            run("mount /dev/mapper/{name} {mount_point}".format(
                name=self.name, mount_point=self.mount_point))
            os.system('echo "[[[[[[[[File system mkfs.xfs finished"> /dev/kmsg')

    def stop(self):
        if not (self.mount_point is None):
            run("rm -rf {mount_point}/*".format(mount_point=self.mount_point))
            run("umount {mount_point}".format(mount_point=self.mount_point))
        run("vdo stop -n {name}".format(name=self.name))

# vdo = VDO('vdo-ram', '/ramdisk')
