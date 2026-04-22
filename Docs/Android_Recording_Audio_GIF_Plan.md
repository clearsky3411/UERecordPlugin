# Android Recording: Audio + GIF Preview 설계 문서 (Living Doc)

## 문서 목적
- 현재 안정화된 Android/Vulkan 녹화 경로를 유지하면서,
  1) **내부 오디오 캡처**,
  2) **GIF thumbnail preview 생성**
  을 단계적으로 추가하기 위한 공통 계획 문서.
- 구현 중 의사결정/진행상황/리스크를 한 곳에서 추적하기 위한 living document.

## 현재 작업 컨텍스트 (2026-04-22)
- 이 문서는 이제 cloud 메모 용도가 아니라, `Codex app + local worktree` 기준 현재 플러그인 상태를 직접 추적하는 기준 문서로 사용한다.
- 현재 저장소 범위는 Unreal 플러그인 `VdjmRecorder` 중심이며, 서버리스/API/서비스 페이지는 이 저장소 밖의 후속 단계로 본다.
- 따라서 문서 우선순위도 `현재 구현된 코드 구조`, `다음 작업 순서`, `완료/미완료 상태`를 먼저 보여주고, 장기 확장 계획은 뒤쪽 참고로 유지한다.
- `EventManager`는 이미 도입되었지만 아직 `BridgeActor`를 즉시 제거한 상태는 아니며, 현재는 오케스트레이션 레이어로 보는 것이 정확하다.
- 서버리스와 서비스 페이지는 원래부터 마지막 구현 단계로 간주하며, 현재 문서에서도 최종 단계로 재배치한다.

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
- Serverless 업로드/서비스 페이지의 실제 구현

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

## 현재 구현 상태 (코드 기준)

| 항목 | 상태 | 메모 |
| --- | --- | --- |
| `EventManager` 분리 | 구현됨 | FlowRuntime/JSON + coarse session state + 브릿지 오케스트레이션 포함 |
| `RecorderController` 초안 | 구현됨 | 월드에서 브릿지 탐색/기본 제어 가능 |
| `RecorderStateObserver` 초안 | 구현됨 | 현재는 EventManager 세션 상태 중심 관찰 |
| Event Flow Runtime/JSON | 구현됨 | Asset clone + JSON import/export 가능 |
| Android Session + 내부 오디오 | 부분 구현 | MediaCodec + Submix listener + mux start gate 코드 존재 |
| Android Vulkan backend | 구현 상세도 높음 | 현재 Android 경로 중 가장 상세하게 구현된 편 |
| Android OpenGL backend | 구현됨, 검증 필요 | EGL 공유 컨텍스트 기반, 실기기 재검증 필요 |
| Windows WMF 경로 | 구현됨, 회귀 위험 높음 | NV12 compute/readback 구조는 있으나 수정 중 흔적이 많아 재검증 필요 |
| Pipeline/Unit 추상화 | 구현됨 | Android는 사실상 단일 unit, WMF는 다단계 구조 |
| UI 연결 | 미구현 | 현재는 Controller/EventManager API까지만 존재 |
| Thumbnail/GIF 후처리 | 미구현 | 설계만 존재 |
| Metadata/권한 | 미구현 | 설계만 존재 |
| Serverless/서비스 페이지 | 미구현 | 최종 단계 |

### 현재 완료된 기반 기능
- [x] `EventManager` 생성/브릿지 바인딩/플로우 실행
- [x] `FlowRuntime` 도입 및 JSON 실행 경로 통합
- [x] `RecorderController` 초안 존재
- [x] `RecorderStateObserver` 초안 존재
- [x] `EVdjmRecordEventResultType`를 `E*` 규칙으로 정리
- [x] 이벤트 관련 파일을 `VdjmEvents` 폴더로 정리
- [x] `EventManager`에 coarse session state 추가
- [x] Android `Vulkan/OpenGL` 백엔드 경로 존재
- [x] WMF `NV12 -> ReadBack -> Encoder` 구조 존재

### 주의 사항
- `EventManager`는 현재 기준으로 **BridgeActor를 대체하는 최종 녹화 실행체가 아니라**, 외부 진입과 이벤트 오케스트레이션을 담당하는 레이어다.
- 실제 녹화 시작/정지, 플랫폼 리소스 생성, 파이프라인 실행은 여전히 `BridgeActor -> Resolver -> Resource -> Pipeline` 경로가 중심이다.
- 따라서 다음 단계는 `BridgeActor 제거`가 아니라, `Observer/Option/UI`가 `BridgeActor` 대신 `EventManager/Controller`를 주 진입점으로 보게 만드는 작업이다.

---

## 현재 기준 큰 단계 계획 (App/Worktree 기준)

> 아래 순서는 현재 저장소 범위와 실제 코드 상태를 반영한 우선순위다.

### TL;DR
0. `EventManager` 기반은 이미 도입됨 (현재 기반 단계)
1. `StateMachine Observer` 재정의
2. `UOptionController` 제작
3. UI 연결
4. Thumbnail 추출 파이프라인
5. Metadata 스키마 + 권한 모델
6. Serverless Video I/O + 서비스 페이지 (최종 단계)

### 설계 원칙 (확정)
- **OptionController는 명령 생성기**: 실제 적용은 Resource에 요청 메시지로 전달
- **Resource는 단일 적용 지점**: 최종 실행값(`FinalFrameRate/FinalBitrate/FinalFilePath`) 보유
- **Session은 스냅샷 소비자**: 실행 중 직접 설정 변경 최소화
- **StateMachine Observer는 Bridge와 분리**: 상태 감시/로그/알림만 담당(제어권 없음)
- **EventManager는 오케스트레이터**: 외부 호출은 점차 EventManager/Controller로 모으고, BridgeActor는 플랫폼 실행체로 남긴다
- **UI는 진입점일 뿐**: 비UI 객체는 특정 UI 타입을 몰라야 하며, 월드에서 필요한 객체를 자동 연결하는 구조를 유지한다

### 현재 우선순위 정리
1. `StateMachine Observer`
   - `RecorderStateObserver`는 현재 `EventManager` 세션 상태를 감시하도록 재정의되었고, `chainInit` 상세 단계는 일반 observer 대상에서 제외했다.
2. `UOptionController`
   - 현재 `RecorderController`의 좁은 옵션 요청 구조를 `OptionController + Resource Apply Layer`로 분리한다.
3. UI 연결
   - UI는 Controller/EventManager를 호출하고, 내부 월드 연결은 비UI 계층에서 자동 해결되도록 한다.
4. Thumbnail
   - 실제 GIF 생성기 이전에 후처리 요청 인터페이스와 결과 메타 구조를 먼저 정리한다.
5. Metadata/권한
   - 업로드 전에도 로컬 산출물 메타와 권한 키 구조를 먼저 정리한다.
6. Serverless/서비스 페이지
   - 이 저장소 밖 후속 단계이며, 계약과 상태 모델만 먼저 정리하고 실제 구현은 마지막으로 둔다.

### 현재 작업 체크리스트
- [x] `EventManager` 기반 도입
- [x] `FlowRuntime`/JSON 경로 통합
- [x] `RecorderController` 초안 존재
- [x] `RecorderStateObserver` 초안 존재
- [x] `StateMachine Observer`를 EventManager/세션 기준으로 재정의
- [x] 이벤트 관련 파일을 `VdjmEvents` 폴더로 정리
- [ ] `UOptionController` 도입
- [ ] Resource Option Apply Layer 구현
- [ ] UI 엔트리/호스트 연결
- [ ] Thumbnail 후처리 인터페이스 정리
- [ ] Metadata 스키마 v1 정리
- [ ] 권한 모델 정리
- [ ] Serverless 계약 정리
- [ ] 서비스 페이지 연동 설계 정리

### 컴포넌트 정의
#### 1) UOptionController (신규 UObject)
- 책임
  - UI 입력 수집
  - `FOptionChangeRequest` 생성/전송
  - 1차 입력 검증(범위/형식)
- 비책임
  - codec/session 직접 제어
  - resolver/preset 직접 변경

#### 2) Resource Option Apply Layer (기존 Resource 확장)
- 책임
  - `ApplyOptionRequest()`로 메시지 적용
  - 2차 검증(디바이스/규칙/권한)
  - 변경 이벤트 브로드캐스트
- 결과
  - effective config 업데이트
  - 다음 snapshot 생성 시 반영

#### 3) StateMachine Observer (신규 UObject, Bridge 분리)
- 책임
  - `ENew/Ready/Running/Waiting/Terminated/Error` 전이 감시
  - 전이 시간, 실패 코드, 재시도 횟수 기록
  - UI/Telemetry 전파
- 비책임
  - 상태 강제 변경(Observer는 감시 전용)

---

### 단계별 상세 플랜

#### Phase 1 — OptionController 제작 (P0)
- 산출물
  - `UOptionController`
  - `FOptionChangeRequest` / `FOptionValidationResult`
  - `IOptionApplier` 인터페이스(Resource 구현체 연결)
- 작업
  - UI 위젯 바인딩(프레임레이트/비트레이트/키프레임/오디오소스/출력정책)
  - 요청 큐(또는 즉시 적용) 정책 확정
  - invalid 요청 rejection + 이유 반환
- 완료 기준
  - UI 조작 시 Resource 값 갱신
  - 적용/거절 로그 일관성

#### Phase 2 — StateMachine Observer (P0)
- 산출물
  - `URecordSessionStateObserver` (독립 UObject)
  - 상태 전이 테이블 + 전이 이벤트 로그 포맷
- 작업
  - Resource/Bridge의 상태 이벤트 구독
  - 전이 guard 검증(불법 전이 차단은 기존 제어부, observer는 탐지/신고)
  - 장애 시나리오 리포트(초기화 실패, mux start 실패, upload 실패 등)
- 완료 기준
  - 상태 전이 타임라인 시각화 가능
  - Error 전이 원인 코드 수집

#### Phase 3 — Thumbnail (P1)
- 산출물
  - preview mp4(loop) 또는 gif 생성 Job
  - 생성 결과 메타(길이/fps/용량/구간)
- 작업
  - mp4 전체에서 일부 구간 추출(예: 10~12%, 40~42%)
  - 저화질/저fps 변환
  - 상호작용 시 반복 재생용 asset 생성
- 완료 기준
  - 업로드 후 썸네일 URL 확보
  - 서비스 페이지 hover/탭 재생 성공

#### Phase 4 — Metadata (P0)
- 산출물
  - 메타 스키마 v1
  - 권한 키(`canView/canEdit/ownerUserId`)
- 작업
  - 영상 식별(`videoId`) + 저장 위치(`storageKey`) + 포맷 정보
  - 권한 검증 훅(소유자 편집 가능)
  - 메타만으로 재생 가능한 조회 API 설계
- 완료 기준
  - 메타만으로 영상 조회/재생
  - 소유자 편집 권한 enforcement

#### Phase 5 — Serverless Video I/O + UI/서비스 페이지 (P0/P1)
- 산출물
  - Presigned Upload API
  - 업로드 완료 콜백 + 메타 기록 + 썸네일 Job enqueue
  - 리스트/상세/편집 페이지
- 작업
  - 상태 머신과 연계된 업로드 상태(`recorded/uploading/processing/ready/failed`)
  - 재시도/멱등성/타임아웃 정책
  - 오류 UX(재시도 버튼/로그 링크)
- 완료 기준
  - E2E: 녹화→업로드→메타/썸네일 생성→서비스 페이지 노출

> 주의: 현재 저장소 기준으로는 Phase 5가 당장 구현 대상이 아니라, **최종 연동 단계**다. 이 문서의 현재 작업 우선순위는 `Observer -> OptionController -> UI -> Thumbnail -> Metadata/권한 -> Serverless/서비스 페이지` 순서로 본다.

---

### 검증 전략 (추가 필수)
- **검증 2단계**
  1) Controller 입력 검증
  2) Resource 적용 검증(Resolver 규칙 + 디바이스 제약)
- **회귀 테스트 축**
  - A/V sync
  - output path 중복 누적
  - 상태 전이 유효성
  - 업로드 실패 복구

### 상태 머신(운영 관점) 제안
`Idle -> Preparing -> Ready -> Recording -> Finalizing -> Uploading -> ProcessingPreview -> ReadyToServe`
`-> Failed` (어느 단계에서든 진입 가능)

각 상태는 다음을 기록:
- `enteredAt`, `leftAt`, `durationMs`
- `reasonCode`, `attempt`, `traceId`

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

---

## 멀티플랫폼/사양 프리셋 설계안 (v3 제안)

### 배경
- `FVdjmRecordGlobalRules`는 계속 공통 규칙으로 유지한다.
- 기존 `PlatformInfoMap`은 하위호환을 위해 유지한다.
- 새 요구사항은 "플랫폼별 + 사양등급별(최저/권장/최고) + 콘텐츠 유형별" 사전 설정 저장이다.

### 핵심 원칙
1. **하위호환 유지**
   - 기존 `TMap<EVdjmRecordEnvPlatform, FVdjmRecordEnvPlatformInfo> PlatformInfoMap` 삭제 금지.
2. **프리셋 분리**
   - 기존 `PlatformInfoMap`은 레거시/기본 fallback.
   - 신규 프리셋 저장소에서 실제 선택값을 우선 결정.
3. **단조성 보장**
   - 저사양 <= 권장 <= 고사양 규칙을 강제한다(해상도/FPS/비트레이트/오디오).
4. **콘텐츠 프로파일링**
   - 콘텐츠 유형(예: Gameplay/Cinematic/UI-heavy)에 따라 프리셋을 분기할 수 있게 한다.

### 제안 데이터 모델
#### A. 기존 유지 (Legacy fallback)
- `PlatformInfoMap[Platform] -> FVdjmRecordEnvPlatformInfo`

#### B. 신규 추가 (Profiled presets)
- `PlatformProfiles[Platform][ContentType][Tier] -> FVdjmRecordEnvPreset`

#### C. 식별 키 제안
- `Platform` : Android / Windows / ...
- `ContentType` : Default, Gameplay, Cinematic, UIHeavy ...
- `Tier` : Low, Recommended, High

### `FVdjmRecordEnvPreset`에 담을 항목(권장)
1. **Video**
   - Width / Height / FPS / Bitrate / Codec / Keyframe interval
2. **Audio**
   - EnableAudio / SampleRate / Channel / Bitrate / AAC profile / DriftToleranceMs / SourceSubmix
3. **RuntimePolicy**
   - RequireAVSync / AllowedDriftMs / StartMuxerWhenBothTracksReady
4. **Postprocess**
   - GIF enable / sampling fps / target size / fallback policy
5. **Compatibility Hint**
   - 최소 API level / 권장 GPU class / known blacklist(optional)

### 프리셋 선택 알고리즘(런타임)
1. 플랫폼 결정 (`TargetPlatform`)
2. 콘텐츠 타입 결정(없으면 `Default`)
3. 단말 capability 점수화 후 tier 결정(Low/Recommended/High)
4. 신규 프리셋 맵에서 정확 매칭 탐색
5. 없으면 순차 fallback
   - Tier fallback: Recommended -> Low -> High
   - Content fallback: SelectedType -> Default
   - 최종 fallback: 기존 `PlatformInfoMap[Platform]`

### 검증 규칙(필수)
#### V1. 구조 검증
- 각 Platform 최소 1개 preset 보유
- 각 ContentType에 Recommended tier 필수

#### V2. 값 검증
- Width/Height/FPS/Bitrate > 0
- Audio enable 시 SampleRate/Channel/AudioBitrate/AAC profile 유효
- Runtime drift >= 0

#### V3. 단조성 검증
- Low <= Recommended <= High
  - Resolution pixel count
  - FPS
  - Video bitrate
  - Audio bitrate

#### V4. 글로벌 룰 교차 검증
- `GlobalRules.MinFrameRate <= preset.FPS <= GlobalRules.MaxFrameRate`
- Global duration/thread 규칙 위반 시 실패

### 마이그레이션 전략
1. 단계 1: 신규 프리셋 필드 추가(읽기만)
2. 단계 2: `GetPlatformInfo`는 유지하되, 새 API `GetBestPreset(...)` 추가
3. 단계 3: Android pipeline부터 `GetBestPreset(...)` 우선 사용
4. 단계 4: 안정화 후 Windows 등으로 확장

### TODO(설계 기반 구현 순서)
- [ ] DataAsset에 `PlatformProfiles` 신규 필드 추가
- [ ] `GetBestPreset(platform, contentType, deviceTier)` API 추가
- [ ] Validation 함수 분리: `ValidatePresetShape/ValidateMonotonicity/ValidateAgainstGlobalRules`
- [ ] Android pipeline에서 preset -> immutable init request snapshot 매핑
- [ ] 실패 로그 표준화(선택 실패 원인: platform/content/tier/fallback 경로)

### 변경 이력 (추가)
- v3 (2026-04-10): 멀티플랫폼/사양 프리셋 설계안 추가, PlatformInfoMap 하위호환 유지 전략 및 검증 규칙 정의
