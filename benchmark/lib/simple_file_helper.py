from lib.pubilc_funs import run
from os.path import join, exists, basename
import os


class SimpleFile:
    fio_payload = "fio --rw=write --bs={bs} --name=vdo --filename={filename} --ioengine=libaio --numjobs={numjobs} --thread --norandommap --iodepth={iodepth} --scramble_buffers=1 --offset=0 --size={size} --direct={direct} --output={output} --output-format=json --buffer_pattern='calgary.tar.gz' --refill_buffers"

    def __init__(self, vdo, ps_monitor, src_file):
        self.vdo = vdo
        self.device = "/dev/mapper/{name}".format(name=vdo.name)
        # self.log_dir = log_dir
        self.ps_monitor = ps_monitor
        self.src_file = src_file
        assert exists(src_file), "Source file '{}' not exists!".format(src_file)
        if self.vdo.mount_point is not None:
            self.filename = join(self.vdo.mount_point, basename(self.src_file))
        else:
            raise ValueError
            # filename = self.device
        # self.output = join(log_dir, 'fio.json')
        # self.fio_package = {'bs': bs, 'filename': filename, 'numjobs': numjobs,
                            # 'iodepth': iodepth, 'size': size, 'direct': direct, 'output': self.output}

    def start(self):
        # payload = self.fio_payload.format(**self.fio_package)
        cpu_index = self.ps_monitor.get_ps_count() + 2
        # cpu_core = "{}-{}".format(
            # cpu_index, str(cpu_index+int(self.fio_package['numjobs'])))
        os.system("cp {} {}".format(self.src_file, self.filename))
        os.system("cp {} {}".format(self.src_file, self.filename + '.1'))
        os.system("cp {} {}".format(self.src_file, self.filename + '.1'))
        os.system("cp {} {}".format(self.src_file, self.filename + '.3'))
        # run("taskset --cpu-list {cpu_core} {payload}".format(
            # cpu_core=cpu_core, payload=payload))
