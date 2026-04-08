# Android Recording: Audio + GIF Preview 설계 문서 (Draft v1)

## 문서 목적
- 현재 안정화된 Android/Vulkan 녹화 경로를 유지하면서,
  1) **내부 오디오 캡처**,
  2) **GIF thumbnail preview 생성**
  을 단계적으로 추가하기 위한 공통 계획 문서.
- 구현 중 의사결정/진행상황/리스크를 한 곳에서 추적하기 위한 living document.

---

## 범위(Scope)
### 포함
- Android 녹화 파이프라인에 내부 오디오(AAC) 추가
- 녹화 완료 후 GIF preview 후처리 파이프라인 추가
- 업로드 연계를 위한 메타데이터 산출 구조 정의

### 제외(현재 단계)
- 마이크 입력 캡처
- VulkanBackend에 추가적인 실시간 후처리(썸네일 생성/디코딩) 로직 탑재
- iOS/Windows 동시 구현

---

## 현재 구조 요약
- `UVdjmRecordAndroidUnit`
  - start 시점에서 `FVdjmAndroidEncoderImpl` 초기화/시작 호출
  - RDG pass에서 frame submit 위임
- `FVdjmAndroidEncoderImpl`
  - 외부 인터페이스/중계기 역할
  - 실제 처리 책임은 `FVdjmAndroidRecordSession`으로 위임
- `FVdjmAndroidRecordSession`
  - codec/muxer lifecycle 관리
  - 비디오 drain 및 write sample 처리
- `FVdjmAndroidEncoderBackendVulkan`
  - 그래픽 백엔드(프레임 공급) 책임만 유지

---

## 아키텍처 원칙
1. **관심사 분리**
   - Graphics Backend(Vulkan/OpenGL) vs Session(encode/mux) vs Postprocess(GIF)
2. **Session 중심 lifecycle**
   - 녹화 1회 = Session 1개
3. **후처리 분리**
   - GIF는 녹화 종료 후 비동기 작업으로 분리
4. **확장 가능한 인터페이스**
   - 기존 공통 인터페이스는 가급적 유지
   - 꼭 필요할 때만 최소 변경

---

## 목표(Goals)
### G1. 내부 오디오(마이크 제외) 지원
- 소스: UE 내부 믹스 출력(Submix tap)
- 인코딩: AAC(AMediaCodec)
- 출력: 기존 mp4에 audio/video 동시 mux

### G2. GIF preview 생성
- 녹화 완료된 mp4 기준 사후 처리
- 구간 샘플링 규칙(%) 기반 프레임 추출
- 저용량/저프레임 GIF 생성

### G3. 업로드용 메타데이터
- 영상/썸네일/GIF와 함께 업로드 가능한 JSON 메타 생성

---

## 의사결정 사항(확정)
- [x] VulkanBackend는 그래픽 백엔드 책임만 유지
- [x] 내부 오디오는 Session 레벨에서 처리
- [x] GIF는 사후처리 파이프라인에서 생성
- [x] 마이크 캡처는 범위 제외
- [x] 사운드 파이프라인 소유자 = `FVdjmAndroidRecordSession` (단일 책임)
- [x] A/V sync는 필수 요구사항 (타협 불가)
- [x] 변경 지점은 최대한 한 곳(Session)으로 집중

---

## 세부 설계안

### A. Audio 파이프라인 (Session 내부)
#### A-1. 상태 머신(초안)
- `Idle -> Prepared -> TracksReady -> Muxing -> Stopping -> Finalized`

#### A-2. 핵심 규칙
- Muxer start 조건 재설계:
  - 기존: 비디오 트랙 준비 즉시 start
  - 변경: **비디오+오디오 트랙 모두 준비 후 start**
- pts 정렬 규칙 정의:
  - video pts 기준 clock
  - audio pts drift 허용 범위 정의(예: ±20ms)

#### A-3. 모듈 분리(초안)
- `FVdjmAndroidInternalAudioSource` (UE mix tap -> PCM)
- `FVdjmAndroidAudioEncoderAac` (PCM -> AAC)
- `FVdjmAndroidMuxCoordinator` (audio/video track add + start + write arbitrate)

#### A-4. 체크리스트
- [ ] UE 내부 오디오 캡처 방식 확정(Submix tap API)
- [ ] AAC codec profile/bitrate/sample rate 기본값 확정
- [ ] audio/video track ready gate 구현
- [ ] mux write 스레드 모델 확정(single thread vs lock-free queue)
- [ ] stop 시 flush/EOS 처리 순서 확정

---

### B. GIF Preview 파이프라인 (Postprocess)
#### B-1. 타이밍
- 녹화 종료 + 파일 finalize 후 실행

#### B-2. 입력
- `final.mp4`
- 구간 규칙(예: 10~12, 22~24, ...)

#### B-3. 처리 순서
1. duration 읽기
2. % 구간 -> 절대시간 변환
3. 구간 디코딩(샘플링 fps 적용)
4. 리사이즈/팔레트/디더링
5. GIF 저장

#### B-4. 출력
- `preview.gif`
- 요약 정보(프레임 수, 길이, 샘플링 규칙)

#### B-5. 체크리스트
- [ ] 구간 규칙 포맷(JSON) 정의
- [ ] 샘플링 fps 및 해상도 정책 확정
- [ ] gif 인코더 라이브러리 선택(내장/외부)
- [ ] 생성 실패 시 fallback 정책(정적 썸네일)

---

### C. 메타데이터 (업로드 연계)
#### C-1. 초안 스키마
```json
{
  "video": {
    "path": "...mp4",
    "duration_ms": 0,
    "width": 0,
    "height": 0,
    "fps": 0,
    "bitrate": 0,
    "has_audio": true
  },
  "preview": {
    "gif_path": "...gif",
    "segments_percent": [[10,12],[22,24]],
    "sampling_fps": 0
  },
  "record": {
    "created_at_unix": 0,
    "session_id": "",
    "sha256": ""
  }
}
```

#### C-2. 체크리스트
- [ ] 메타 생성 타이밍 확정(Stop 직후/업로드 직전)
- [ ] 해시 계산 정책 확정(파일 완료 후)
- [ ] 서버 업로드 API 계약 반영

---

## 리스크 및 대응
1. **A/V sync 드리프트**
   - 대응: 공통 clock, drift 모니터링 로그 추가
2. **단말별 codec/mux 차이**
   - 대응: device profile별 안전 기본값
3. **GIF 생성 시간/배터리 비용**
   - 대응: 백그라운드 low-priority job + timeout
4. **실패 복구**
   - 대응: mp4 성공/GIF 실패 분리 저장 및 재시도 정책

---

## 마일스톤
### M1. 설계 확정
- [ ] Audio source/encoder/mux start gate 상세 설계 리뷰
- [ ] GIF postprocess 규칙/성능 목표 합의

### M2. Audio 구현
- [ ] 내부 오디오 캡처 + AAC 인코딩 + mux 연동
- [ ] 3회 연속 녹화 안정성 검증

### M3. GIF 구현
- [ ] 구간 샘플링 + gif 생성
- [ ] 생성 시간/용량 튜닝

### M4. 메타데이터/업로드 연계
- [ ] 메타 파일 생성
- [ ] 업로드 API 연결 및 실패 재시도

---

## 오픈 질문
- [ ] 내부 오디오 캡처를 엔진 레벨에서 어디서 탭할지(정확 API)
- [ ] GIF 인코더를 네이티브로 둘지, 별도 유틸/서버로 둘지
- [ ] 저사양 단말 fallback 기준(해상도/fps/길이)

---

## 우선순위(P0/P1/P2)
### P0 (필수)
- A/V sync 보장 (내부 오디오 + 비디오 타임라인 일치)
- `FVdjmAndroidRecordSession` 단일 소유 구조 유지 (오디오 파이프라인 책임 집중)
- 오디오 트랙/비디오 트랙 ready gate 후 mux start

### P1 (중요)
- GIF preview 사후처리 구현
- 업로드 연계 메타데이터 생성

### P2 (추가)
- 저사양 단말 최적화 및 fallback 고도화
- GIF 품질/용량 자동 튜닝

---

## 요구 기간 / 담당자
- 요구 기간: **2일**
- 담당자: **vdjm**

---

## 완료 기준(Definition of Done)
- 구현 완료 후 **vdjm 직접 실행 검증**
- 실행 결과 로그(성공/실패/에러 케이스) 공유
- 공유 로그 기준으로 최종 수정/확정

---

## 변경 이력
- v1 (2026-04-08): 초안 생성
- v2 (2026-04-08): 우선순위/요구기간/담당자/완료기준 반영, Session 단일소유 원칙 명시
