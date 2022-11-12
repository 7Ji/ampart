import dataclasses

class Size:
    @staticmethod
    def from_human_readable(size: str) -> int:
        match (suffix := size[-1].lower()):
            case 'B':
                multiply = 1
            case 'K':
                multiply = 0x400
            case 'M':
                multiply = 0x100000
            case 'G':
                multiply = 0x40000000
            case 'T':
                multiply = 0x10000000000
            case _:
                if suffix <= '9' and suffix >= '0':
                    return int(size)
                else:
                    raise ValueError(f'Invalid suffix in string to conver to size: {suffix}')
        return int(size[:-1]) * multiply

class Partition:
    def __init__(self, name:str, size:int, masks:int):
        self._name = name
        self._size = size
        self._masks = masks

    @classmethod
    def from_profile(cls, partition: dict):
        name = partition['name']
        size = partition['size']
        
        masks = partition['masks']

    @property
    def name(self):
        return self._name

    @property
    def size(self):
        return self._size

    @property
    def _masks(self):
        return self._masks
    



if __name__ == '__main__':
    print("WIP, DO NOT USE")
    pass