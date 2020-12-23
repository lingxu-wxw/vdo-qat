from lib.pubilc_funs import run
from os.path import join


class FIO:
    FIO_BIN = "/home/zongpu/packages/fio/fio"
    # FIO_BIN = "fio"
    fio_payload = FIO_BIN + " --rw=write --bs={bs} --name=vdo --filename={filename} --ioengine=libaio " + \
                            "--numjobs={numjobs} --iodepth={iodepth} " + \
                            "--size={size} --direct={direct} --output={output} --output-format=json " + \
                            "--buffer_pattern='calgary.tar.gz' --refill_buffers"
    # --rw=randwrite
    # --scramble_buffers=1
    # --thread
    # --norandommap
    # --offset=0

    # fio_payload = "fio --rw=write --bs={bs} --name=vdo --filename={filename} --ioengine=libaio --numjobs={numjobs} --thread --norandommap --iodepth={iodepth} --scramble_buffers=1 --offset=0 --size={size} --direct={direct} --output={output} --output-format=json --buffer_pattern='calgary.tar.gz' --refill_buffers"


    def __init__(self, vdo, ps_monitor, log_dir, bs='1024*1024', numjobs='1', iodepth='16', size='16g', direct='0'):
        self.vdo = vdo
        self.device = "/dev/mapper/{name}".format(name=vdo.name)
        self.log_dir = log_dir
        # self.ps_monitor = ps_monitor
        if self.vdo.mount_point is not None:
            filename = join(self.vdo.mount_point, 'fio')
        else:
            filename = self.device
        self.output = join(log_dir, 'fio.json')
        self.fio_package = {'bs': bs, 'filename': filename, 'numjobs': numjobs,
                            'iodepth': iodepth, 'size': size, 'direct': direct, 'output': self.output}
        run(self.FIO_BIN + " --version")
        print("Make sure FIO version is: fio-3.23-dirty!!!")

    def start(self):
        payload = self.fio_payload.format(**self.fio_package)
        print("Running fio: {}".format(payload))
        # cpu_index = self.ps_monitor.get_ps_count() + 2
        # cpu_core = "{}-{}".format(
            # cpu_index, str(cpu_index+int(self.fio_package['numjobs'])))
        # run("taskset --cpu-list {cpu_core} {payload}".format(
            # cpu_core=cpu_core, payload=payload))
        run("{payload}".format(payload=payload))
