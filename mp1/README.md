# MP1 · Reliable Data Transfer

오류가 발생하는 채널에서 파일을 정확하게 전송하는 sender를 구현했습니다.
`sender_20211605.cc`에 CRC-32 계산, 프레임 구성과 응답에 따른 재전송을 구현하고
채널 오류에 대응하며 전송 크기를 조절합니다.

`netsim.h`, `netsim_lib.cc`, Linux 실행 파일 `netsim`과 작은 검증 입력은 수업 제공 자료입니다.
동일 이름의 후보 두 개 중 수정 시각이 늦은 `mp1_testcases/`의 구현을 보관했습니다.

## 빌드와 실행

Linux x86-64 / WSL에 g++가 필요합니다.

```bash
g++ -O2 -o sender_20211605 sender_20211605.cc netsim_lib.cc
chmod +x netsim
./netsim ./sender_20211605 --input testdata/tiny1k.bin --output out.rx   --ber 0.001 --seed 42 --max_bytes 100000
cmp testdata/tiny1k.bin out.rx
```

`--max_bytes`는 채널에서 허용하는 총 전송량입니다. 필요하면 입력 크기와 오류율에 맞게 늘립니다.
정확한 데이터 일치는 `cmp`가 출력 없이 종료하는지 확인합니다.
입력 파일을 바꾸면 다른 파일도 같은 방식으로 검증할 수 있습니다.

## 문서

- [과제 설명과 평가 기준](docs/assignment.pdf)
- 과제 제공 시뮬레이터 바이너리는 Linux 환경용입니다.

2026-10-08: 1/10/100/1000바이트 입력 4개가 모두 성공했고 수신 파일이 원본과 일치했습니다. 검증 조건은 BER 0.001, seed 42입니다.
