from lib.psmonitor import PSMonitor
from lib.vdo_helper import VDO
from lib.fio_helper import FIO
from lib.fio_parser import fio_print
from lib.grid_search import Grid_Search
from time import sleep


def call_fun(vdo_package):
    vdo = VDO('vdo-ram')
    vdo.create('40G', vdoAckThreads=vdo_package[0], vdoBioThreads=vdo_package[1], vdoCpuThreads=vdo_package[2],
               vdoHashZoneThreads=vdo_package[3], vdoLogicalThreads=vdo_package[4], vdoPhysicalThreads=vdo_package[5])
    ps_monitor = PSMonitor('./', log_freq=0.01, last_time=-1)
    ps_monitor.set_cpu_affinity()
    fio = FIO(vdo, ps_monitor, './', bs=16*1024, size='1g', numjobs=2)
    ps_monitor.start()
    fio.start()
    ps_monitor.end()
    vdo.remove()
    return fio_print()


grid_search = Grid_Search(call_fun, [1,2,6], 6)
print('Counts:', grid_search.get_count())
grid_search.start(verbose=True)

# from itertools import product
# base = [3,4,5]
# result = product(base, repeat=6)
# print(len(set(result)))
