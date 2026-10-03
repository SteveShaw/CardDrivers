import csv
import struct
import sys
from decimal import Decimal


SYMBOL_EXPIRATION_STRUCT = struct.Struct("=5scBB")

def pack_symbol_expiration(symbol, expiration):
    symbol_bytes = symbol.encode("ascii")

    if len(symbol_bytes) > 5:
        raise ValueError(f"Symbol exceeds 5 characters: {symbol!r}")

    if (
        len(expiration) != 5
        or not expiration.isascii()
        or not expiration[1:].isdigit()
    ):
        raise ValueError(f"Invalid expiration block: {expiration!r}")

    return SYMBOL_EXPIRATION_STRUCT.pack(
        symbol_bytes.ljust(5, b" "),
        expiration[0].encode("ascii"),
        int(expiration[1:3]),
        int(expiration[3:5]),
    )


def parse_csv(filename):
    records = [[] for _ in range(96)]
    

    with open(filename, newline="", encoding="utf-8-sig") as file:
        for row in csv.DictReader(file):
            line_number = int(row["MulticastLineNumber"])
            records[line_number - 1].append([pack_symbol_expiration(row["Symbol"], row["ExpirationBlock"]), int(Decimal(row["StrikePrice"]) * 10_000)])

    for index, record_list in enumerate(records):
        print(f"Record {index + 1}: {len(record_list)}")
        with open(f"opra_symbol_line_{index + 1}.dat", "wb") as f:
            f.write(struct.pack("=I", len(record_list)))
            for record in record_list:
                f.write(record[0])
                f.write(struct.pack("=I", record[1]))




if __name__ == "__main__":
    parse_csv(sys.argv[1])
