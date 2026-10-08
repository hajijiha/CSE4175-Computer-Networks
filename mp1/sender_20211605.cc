// sender_20211605.cc
//
// Stop-and-Wait ARQ sender with CRC-32 over a lossy channel.
//
// ---------------------------------------------------------------------------
// 적응 전략 (Adaptive payload size)
// ---------------------------------------------------------------------------
//  비용 모델:  cost/유용바이트 = (P + 6 + K) / (P * (1 - p_f))
//              p_f = 1 - (1 - BER)^(8*(P+4))
//  ln(1-BER) ≈ -BER 근사로 미분하면 최적 P 는 다음 식의 해:
//      P^2 + 256P - 32/BER = 0
//   => P* = -128 + sqrt(16384 + 32/BER)
//  참고값 BER 1e-7→17761, 1e-6→5542, 1e-5→1668, 1e-4→455, 1e-3→92.
//
//  Sender 는 BER 을 모르므로 ACK/NAK 관측으로만 추정한다 (정보사용규정 준수).
//   - 누적 통계(총 프레임수, NAK수, 노출 비트수)로 BER 추정.
//   - BER ≈ -ln(1 - nak_rate) / avg_bits (NAK이 있을 때)
//   - NAK이 0일 때: BER ≈ 0.5 / total_bits (Bayesian posterior estimate)
//   - 추정 BER에서 닫힌 형식 식으로 P* 계산하여 current_P로 사용.
//
//  사용하는 정보는 (1) 입력 파일 내용, (2) 내부 상태, (3) send_frame
//  반환값 뿐이다. netsim 인자/환경 등은 일절 사용하지 않는다.
// ---------------------------------------------------------------------------
// 프레임 포맷
//   [size:2B BE][payload:P bytes][CRC32:4B BE], 총 P+6 바이트.
//   CRC 는 (size 2B + payload P B) = P+2 바이트에 대해 계산.
// ---------------------------------------------------------------------------
// CRC-32 사양 (과제 명세 §5)
//   poly=0x04C11DB7, MSB-first, init=0, no input/output reflection,
//   no final XOR.
// ---------------------------------------------------------------------------

#include "netsim.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------- CRC-32 (poly 0x04C11DB7, MSB-first, init=0, no refl, no xorout) ----------
static uint32_t crc_table[256];

static void crc_init() {
    for (int i = 0; i < 256; i++) {
        uint32_t c = (uint32_t)i << 24;
        for (int j = 0; j < 8; j++) {
            c = (c & 0x80000000u) ? ((c << 1) ^ 0x04C11DB7u) : (c << 1);
        }
        crc_table[i] = c;
    }
}

static uint32_t crc_compute(const uint8_t *data, size_t len) {
    uint32_t c = 0;
    for (size_t i = 0; i < len; i++) {
        c = (c << 8) ^ crc_table[((c >> 24) ^ data[i]) & 0xff];
    }
    return c;
}

// ---------- Optimal payload from estimated BER (closed form) ----------
static int optimal_payload(double ber) {
    if (ber < 1e-12) return 60000;
    double P = -128.0 + std::sqrt(16384.0 + 32.0 / ber);
    if (P < 10.0) P = 10.0;
    if (P > 60000.0) P = 60000.0;
    return (int)(P + 0.5);
}

// ---------- Cumulative statistics ----------
static int cum_count = 0;
static int cum_naks = 0;
static double cum_bits = 0.0;

static void cum_push(int is_nak, int P) {
    cum_count++;
    cum_naks += is_nak;
    cum_bits += 8.0 * (P + 4);
}

static double estimate_ber() {
    if (cum_count < 3) return -1.0;
    double nak_rate = (double)cum_naks / cum_count;
    double avg_bits = cum_bits / cum_count;

    if (nak_rate <= 0.0) {
        // Bayesian posterior estimate: BER ≈ 0.5 / total_bits
        return 0.5 / cum_bits;
    }
    if (nak_rate >= 0.99) {
        return 1e-2;
    }
    return -std::log(1.0 - nak_rate) / avg_bits;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s input_file\n", argv[0]);
        return 1;
    }

    std::FILE *f = std::fopen(argv[1], "rb");
    if (!f) {
        std::fprintf(stderr, "Cannot open '%s'\n", argv[1]);
        return 1;
    }
    std::fseek(f, 0, SEEK_END);
    long fsize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (fsize < 0) { std::fclose(f); return 1; }

    uint8_t *data = (uint8_t *)std::malloc((size_t)fsize);
    if (!data) { std::fclose(f); return 1; }
    if (fsize > 0 && (long)std::fread(data, 1, (size_t)fsize, f) != fsize) {
        std::fclose(f);
        std::fprintf(stderr, "Read error\n");
        return 1;
    }
    std::fclose(f);

    crc_init();

    static uint8_t frame_buf[65535 + 6];

    int current_P = 1024;
    long pos = 0;
    long total_frames = 0;
    long total_bytes = 0;

    while (pos < fsize) {
        int remaining = (int)std::min<long>(fsize - pos, 60000);
        int P = std::min(current_P, remaining);
        if (P < 1) P = 1;

        frame_buf[0] = (uint8_t)((P >> 8) & 0xff);
        frame_buf[1] = (uint8_t)(P & 0xff);
        std::memcpy(frame_buf + 2, data + pos, (size_t)P);
        uint32_t crc = crc_compute(frame_buf, (size_t)(P + 2));
        frame_buf[P + 2] = (uint8_t)((crc >> 24) & 0xff);
        frame_buf[P + 3] = (uint8_t)((crc >> 16) & 0xff);
        frame_buf[P + 4] = (uint8_t)((crc >>  8) & 0xff);
        frame_buf[P + 5] = (uint8_t)( crc        & 0xff);

        int r = send_frame(frame_buf, P + 6);
        if (r == NETSIM_ERROR) {
            std::fprintf(stderr, "[sender] NETSIM_ERROR\n");
            std::free(data);
            return 1;
        }

        total_frames++;
        total_bytes += P + 6;

        int is_nak = (r == NETSIM_NAK) ? 1 : 0;
        cum_push(is_nak, P);

        if (r == NETSIM_ACK) {
            pos += P;
        }

        // ---------- Adaptive P ----------
        double ber = estimate_ber();
        if (ber > 0) {
            int opt = optimal_payload(ber);
            current_P = opt;
            if (r == NETSIM_NAK && cum_count < 8) {
                int cap = std::max(10, (int)(P * 0.5));
                if (current_P > cap) current_P = cap;
            }
        } else {
            if (r == NETSIM_NAK) {
                current_P = std::max(10, current_P / 2);
            }
        }

        if (current_P < 1) current_P = 1;
        if (current_P > 60000) current_P = 60000;
    }

    std::fprintf(stderr,
                 "[sender] done file=%ld frames=%ld bytes=%ld cost=%ld\n",
                 fsize, total_frames, total_bytes,
                 total_bytes + 250L * total_frames);
    std::free(data);
    return 0;
}
