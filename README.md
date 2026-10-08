# CSE4175 · 컴퓨터네트워크

오류가 있는 채널에서의 데이터 전송과 변화하는 네트워크에서의 라우팅을 구현한 Machine Problem 모음입니다.

| 항목 | 내용 |
|---|---|
| 학교 | 서강대학교 |
| 학기 | 2026-1 |
| 과목코드 | CSE4175 |
| 개발 환경 | C++ / GCC / Network Simulator |
| 공통 자료 | [강의계획서](docs/syllabus.pdf) |

## 프로젝트

| 순서 | 프로젝트 | 구현 내용 |
|---|---|---|
| [mp1](mp1/README.md) | Reliable Data Transfer | CRC-32와 ARQ 기반 신뢰성 있는 전송 |
| [mp2](mp2/README.md) | Dynamic Routing | 네트워크 변화에 대응하는 분산 라우팅 |

## 저장소 구조

```text
CSE4175-Computer-Networks/
├── README.md
├── docs/syllabus.pdf
├── mp1/  # Reliable Data Transfer
├── mp2/  # Dynamic Routing
```

프로젝트별 README에서 구현 파일, 실행 명령, 관련 문서와 검증 범위를 확인할 수 있습니다.
학기와 과목코드는 해당 학기의 강의계획서를 기준으로 기록했습니다.

## 자료 출처

제출본과 수업 제공 스켈레톤·테스트 도구를 함께 정리했습니다. 제공 코드와 팀 코드의
저작권 표시를 유지하고, 직접 구현한 부분은 프로젝트별 README에 구분했습니다.

[정리 시 검증 기록](docs/verification.md)
