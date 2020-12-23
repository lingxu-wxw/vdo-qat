from itertools import product
import time


class Grid_Search():
    def __init__(self, call_fun, payload_items, lens, baseline=0):
        self.payloads = []
        self.baseline = baseline
        self.best_payload = []
        for c in product(payload_items, repeat=lens):
            self.payloads.append(c)
        self.call_fun = call_fun

    def get_count(self):
        return len(self.payloads)

    def start(self, verbose=False):
        result = self.baseline
        count = 0
        for payload in self.payloads:
            count += 1
            start_time = time.time()
            result_temp = self.call_fun(payload)
            end_time = time.time()
            eta = (self.get_count()-count)*(end_time-start_time)
            if result_temp > result:
                result = result_temp
                self.best_payload = payload
            if verbose:
                print("====================")
                print("Best result:", result)
                print("Best payload:", self.best_payload)
                print("Time spending", (end_time-start_time), "sec")
                print("Count:", count, "{}%".format(
                    count*100//self.get_count()), "ETA", "{} min".format(eta/60))
                print("====================")
        return result
