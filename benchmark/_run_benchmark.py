from lib.psmonitor import PSMonitor
from lib.vdo_helper import VDO
from lib.fio_helper import FIO
from lib.fio_parser import fio_print
from lib.grid_search import Grid_Search
from config import vb_config
from os.path import join, exists
from os import makedirs


def call_fun(vdo_package):
    log_dir = join("logs", vb_config.test_name, '-'.join(str(i)
                                                         for i in vdo_package))
    if not exists(log_dir):
        makedirs(log_dir)
    vdo = VDO('vdo-benchmark')
    vdo.create('100G', device=vb_config.device, vdoAckThreads=vdo_package[0], vdoBioThreads=vdo_package[1], vdoCpuThreads=vdo_package[2],
               vdoHashZoneThreads=vdo_package[3], vdoLogicalThreads=vdo_package[4], vdoPhysicalThreads=vdo_package[5])
    ps_monitor = PSMonitor(log_dir, log_freq=vb_config.log_freq, last_time=-1)
    ps_monitor.set_cpu_affinity()
    fio = FIO(vdo, ps_monitor, log_dir, bs=16*1024, size='2g', numjobs=2)
    ps_monitor.start()
    fio.start()
    ps_monitor.end()
    vdo.remove()
    return fio_print(join(log_dir, 'fio.json'))


grid_search = Grid_Search(call_fun, vb_config.grid, 6)
print('Counts:', grid_search.get_count())
grid_search.start(verbose=True)
