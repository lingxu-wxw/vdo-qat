import json


def fio_print(fio_log):
    agg = get_fio_speed(fio_log)
    print(agg / 1024, 'MB/s')
    return agg / 1024

def get_fio_speed(fio_log):
    with open(fio_log) as f:
        fio = json.load(f)
        agg = 0
        for job in fio['jobs']:
            agg += job['write']['bw']
            agg += job['read']['bw']
        return agg
