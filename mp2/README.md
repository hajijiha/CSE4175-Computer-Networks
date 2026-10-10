# MP2 · Dynamic Routing

링크 비용 변화·연결 단절·새 연결에 대응하는 라우터를 구현했습니다.
`router_20211605.cc`가 라우터 상태, 메시지 처리와 경로 선택을 담당하고,
`netsim2_lib.cc`가 네트워크 시뮬레이션 런타임을 제공합니다.

## 빌드와 실행

Linux/WSL의 g++로 컴파일합니다.

```bash
g++ -O2 -o router_20211605 router_20211605.cc netsim2_lib.cc
./router_20211605 --scenario val1.scn
./router_20211605 --scenario val2.scn
./router_20211605 --scenario val3.scn
./router_20211605 --scenario val4.scn
```

시뮬레이션 종료 시 stderr에 전송 성공 여부, 패킷 수, 경로 비용과 제어 메시지 비용이 출력됩니다.

## 검증

2026-10-08 Ubuntu에서 컴파일한 제출 구현으로 네 가지 공개 시나리오를 실행했습니다.

| 시나리오 | 전달 / 전체 | 유실 | 결과 |
|---|---:|---:|---|
| val1 | 6 / 6 | 0 | SUCCESS |
| val2 | 9 / 9 | 0 | SUCCESS |
| val3 | 21 / 21 | 0 | SUCCESS |
| val4 | 11 / 11 | 0 | SUCCESS |

검증 범위는 공개 시나리오 `val1`~`val4`입니다.

## 문서

- [과제 설명](docs/mp2.pdf)
- [제공 패키지 안내](README.txt)
- 헤더, 시뮬레이터와 `val*.scn`은 수업 제공 자료입니다.
