# VdjmRecord Metadata Schema v1

## 문서 목적
- 이 문서는 `UVdjmRecordMediaManifest`와 `UVdjmRecordMetadataStore`가 저장/로드하는 JSON 구조를 v1 계약으로 고정하기 위한 문서다.
- 목표는 metadata JSON 하나만 보고도 `재생`, `목록 표시`, `권한 판단`, `업로드/동기화`, `edit 진입`을 준비할 수 있게 하는 것이다.
- 이 문서는 현재 코드 기준이다. 새 필드를 추가할 때는 C++ serializer/parser와 이 문서를 같이 갱신한다.

## 핵심 구분

| 파일 | 역할 | 진실의 범위 |
| --- | --- | --- |
| `*.vdjm.json` | 개별 녹화 결과 manifest | 한 media의 명함, 재생 정보, 권한, 무결성 |
| `VdjmRecordMediaRegistry.json` | 앱 내부 목록 registry | manifest 목록 캐시, 파일 존재 상태, 목록 표시용 요약 |
| `AppState` JSON | 앱/유저 상태 | 서버 endpoint, 마지막 화면 상태, preview 선택 상태 |

## Manifest v1 개요

Manifest는 개별 media의 source of truth다. registry는 manifest를 빠르게 목록화하기 위한 캐시다.

```json
{
  "schema_version": 1,
  "record": {},
  "media": {},
  "playback": {},
  "preview": {},
  "publication": {},
  "authority": {},
  "integrity": {}
}
```

## Manifest Root

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema_version` | number | yes | 현재 `1`. parser는 없으면 `1`로 본다. |
| `record` | object | yes | 녹화 자체의 식별 정보. |
| `media` | object | yes | 로컬 파일과 영상 속성. |
| `playback` | object | yes | 앱/웹/서버에서 영상을 찾는 정보. |
| `preview` | object | yes | 썸네일/프리뷰 반복 구간 정보. |
| `publication` | object | yes | Android MediaStore 또는 외부 노출 상태. |
| `authority` | object | yes | owner/user/token/key 같은 권한 판단용 정보. |
| `integrity` | object | yes | hash/signature 같은 무결성 정보. |

## `record`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `record_id` | string | yes | media의 안정적인 id. registry upsert 기준 중 하나다. |
| `created_unix_time` | number | yes | UTC Unix timestamp. 정렬/표시 기준으로 사용한다. |
| `source_app` | string | no | 기본값 `vdjm`. 생성 앱/도구 식별용. |

## `media`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `output_file_path` | string | yes | 로컬 원본 파일 경로. |
| `metadata_file_path` | string | yes | 이 manifest 파일 경로. 없으면 로드 시 fallback 경로가 들어간다. |
| `file_size_bytes` | number | yes | 원본 media 파일 크기. 없으면 `-1`. |
| `recorded_frame_count` | number | no | 녹화된 frame count. |
| `platform` | string | yes | `EVdjmRecordEnvPlatform` 문자열. 예: `EVdjmRecordEnvPlatform::EAndroid`. |
| `mime_type` | string | yes | 예: `video/avc`, `video/mp4`. |
| `width` | number | yes | video width. |
| `height` | number | yes | video height. |
| `fps` | number | yes | video frame rate. |
| `bitrate` | number | yes | video bitrate. |

## `playback`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `locator_type` | string | yes | `local_path`, `content_uri`, `remote_url` 같은 위치 타입. 기본값 `local_path`. |
| `locator` | string | yes | 실제 재생 위치. 없으면 로드 시 `media.output_file_path`를 fallback으로 쓴다. |
| `stream_token_id` | string | no | 서버/웹 스트리밍 토큰 id. 실제 secret이 아니라 id만 둔다. |
| `expires_unix_time` | number | no | 재생 토큰 만료 시각. `0`이면 미정 또는 만료 없음으로 본다. |

## `preview`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `thumbnail_file_path` | string | no | 정지 썸네일 경로. 아직 없을 수 있다. |
| `preview_clip_file_path` | string | no | 별도 preview clip 경로. 없으면 원본 media에서 구간 반복한다. |
| `preview_clip_mime_type` | string | no | 기본값 `video/mp4`. |
| `start_time_sec` | number | yes | preview 시작 시간. 최소 `0.0`으로 보정된다. |
| `duration_sec` | number | yes | preview 반복 길이. 최소 `0.1`, 기본 `3.0`. |
| `status` | string | yes | `EVdjmRecordMediaPreviewStatus` 문자열. |
| `error_reason` | string | no | preview 준비 실패 이유. |

Preview source 선택 우선순위는 현재 코드 기준으로 다음과 같다.

1. `preview.preview_clip_file_path`
2. `media.output_file_path`
3. `playback.locator`
4. `publication.content_uri`

## `publication`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `status` | string | yes | `EVdjmRecordMediaPublishStatus` 문자열. |
| `content_uri` | string | no | Android MediaStore content uri. |
| `display_name` | string | no | 외부 노출 이름. |
| `relative_path` | string | no | MediaStore 상대 경로 또는 외부 저장소 분류 경로. |
| `error_reason` | string | no | publish 실패 이유. |

`status` 값:
- `EVdjmRecordMediaPublishStatus::ENotStarted`
- `EVdjmRecordMediaPublishStatus::EPublishing`
- `EVdjmRecordMediaPublishStatus::EPublished`
- `EVdjmRecordMediaPublishStatus::EFailed`
- `EVdjmRecordMediaPublishStatus::ESkippedUnsupportedPlatform`

## `authority`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `role` | string | yes | `EVdjmRecordManifestAuthorityRole` 문자열. |
| `user_id` | string | no | owner/developer/master가 누구인지 식별하는 공개 id. |
| `token_id` | string | no | 권한 검증용 토큰 id. 실제 secret을 넣지 않는다. |
| `key_id` | string | no | 서명/검증 key id. |
| `requires_auth` | bool | yes | 기본 `true`. 외부 접근 시 인증 필요 여부. |

현재 role 값:
- `EVdjmRecordManifestAuthorityRole::EUndefined`
- `EVdjmRecordManifestAuthorityRole::EMaster`
- `EVdjmRecordManifestAuthorityRole::EDeveloper`

### 권한키 설계 초안

아직 확정 구현은 아니지만 v1에서 의도하는 역할은 다음과 같다.

| 개념 | manifest 필드 | 목적 |
| --- | --- | --- |
| Owner/User id | `authority.user_id` | 이 media를 만든 주체 또는 편집 가능한 주체를 가리킨다. |
| Role | `authority.role` | master/developer 같은 로컬 권한 레벨을 표현한다. |
| Token id | `authority.token_id`, `playback.stream_token_id` | 서버나 웹에서 실제 토큰을 찾아 검증하기 위한 id다. secret 자체가 아니다. |
| Key id | `authority.key_id`, `integrity.key_id` | signature 검증에 쓸 공개 key 또는 key slot id다. |
| Requires auth | `authority.requires_auth` | 공개 재생인지 인증 필요 재생인지 빠르게 판단한다. |

원칙:
- manifest에는 secret 원문을 넣지 않는다.
- 웹 재생용 토큰과 편집 권한 토큰은 분리하는 것이 안전하다.
- `canView`, `canEdit` 같은 계산 결과는 서버/앱에서 평가한 runtime 결과로 보고, manifest v1에는 원천 정보만 둔다.
- 이후 필요하면 `authority.capabilities` 배열을 v2 후보로 추가한다.

## `integrity`

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `video_sha256` | string | no | 원본 media hash. |
| `metadata_sha256` | string | no | canonical metadata hash. |
| `signature` | string | no | 서버/앱 서명값. |
| `key_id` | string | no | 검증 key id. 비어 있으면 `authority.key_id`와 연결된다. |

현재는 필드만 준비되어 있고, hash/signature 생성 정책은 후속 단계다.

## Manifest 예시

```json
{
  "schema_version": 1,
  "record": {
    "record_id": "RecordingOutput_20260504_153000",
    "created_unix_time": 1777885800,
    "source_app": "vdjm"
  },
  "media": {
    "output_file_path": "/storage/emulated/0/Android/data/com.vdjm.recordtest/files/RecordingOutput.mp4",
    "metadata_file_path": "/storage/emulated/0/Android/data/com.vdjm.recordtest/files/RecordingOutput.vdjm.json",
    "file_size_bytes": 804834,
    "recorded_frame_count": 180,
    "platform": "EVdjmRecordEnvPlatform::EAndroid",
    "mime_type": "video/avc",
    "width": 616,
    "height": 1280,
    "fps": 30,
    "bitrate": 1892352
  },
  "playback": {
    "locator_type": "local_path",
    "locator": "/storage/emulated/0/Android/data/com.vdjm.recordtest/files/RecordingOutput.mp4",
    "stream_token_id": "",
    "expires_unix_time": 0
  },
  "preview": {
    "thumbnail_file_path": "",
    "preview_clip_file_path": "",
    "preview_clip_mime_type": "video/mp4",
    "start_time_sec": 0.0,
    "duration_sec": 3.0,
    "status": "EVdjmRecordMediaPreviewStatus::EReady",
    "error_reason": ""
  },
  "publication": {
    "status": "EVdjmRecordMediaPublishStatus::EPublished",
    "content_uri": "content://media/external/video/media/1000008062",
    "display_name": "RecordingOutput.mp4",
    "relative_path": "Movies/VdjmRecord",
    "error_reason": ""
  },
  "authority": {
    "role": "EVdjmRecordManifestAuthorityRole::EDeveloper",
    "user_id": "developer-local",
    "token_id": "dev-token-id",
    "key_id": "local-dev-key",
    "requires_auth": true
  },
  "integrity": {
    "video_sha256": "",
    "metadata_sha256": "",
    "signature": "",
    "key_id": "local-dev-key"
  }
}
```

## Registry v1 개요

Registry는 manifest 목록을 빠르게 띄우기 위한 앱 내부 캐시다.

```json
{
  "schema_version": 1,
  "generated_unix_time": 1777885800,
  "registry_file_path": ".../VdjmRecordMediaRegistry.json",
  "entries": []
}
```

## Registry Root

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema_version` | number | yes | 현재 `1`. |
| `generated_unix_time` | number | yes | registry 저장 시각. |
| `registry_file_path` | string | yes | registry 파일 경로. |
| `entries` | array | yes | 목록 표시용 entry 배열. |

## Registry Entry

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `record_id` | string | manifest record id. |
| `output_file_path` | string | 원본 media 경로. |
| `metadata_file_path` | string | manifest 경로. |
| `playback_locator_type` | string | 재생 위치 타입. |
| `playback_locator` | string | 재생 위치. |
| `thumbnail_file_path` | string | 정지 썸네일 경로. |
| `preview_clip_file_path` | string | preview clip 경로. |
| `preview_clip_mime_type` | string | preview clip mime. |
| `preview_error_reason` | string | preview 실패 이유. |
| `published_content_uri` | string | Android MediaStore content uri. |
| `published_display_name` | string | 외부 표시 이름. |
| `published_relative_path` | string | 외부 상대 경로. |
| `video_mime_type` | string | video mime. |
| `last_error_reason` | string | 마지막 오류. |
| `created_unix_time` | number | 생성 시각. |
| `file_size_bytes` | number | 파일 크기. |
| `recorded_frame_count` | number | frame count. |
| `width` | number | video width. |
| `height` | number | video height. |
| `fps` | number | frame rate. |
| `bitrate` | number | bitrate. |
| `preview_start_time_sec` | number | preview 시작 시간. |
| `preview_duration_sec` | number | preview 길이. |
| `platform` | string | platform enum string. |
| `media_publish_status` | string | publish status enum string. |
| `registry_status` | string | registry status enum string. |
| `preview_status` | string | preview status enum string. |
| `output_file_exists` | bool | 원본 파일 존재 여부. refresh 시 갱신된다. |
| `metadata_file_exists` | bool | manifest 존재 여부. refresh 시 갱신된다. |
| `is_deleted` | bool | 앱 내부 삭제 표시. |

`registry_status` 값:
- `EVdjmRecordMediaRegistryEntryStatus::EUnknown`
- `EVdjmRecordMediaRegistryEntryStatus::EAvailable`
- `EVdjmRecordMediaRegistryEntryStatus::EMissingMedia`
- `EVdjmRecordMediaRegistryEntryStatus::EMissingMetadata`
- `EVdjmRecordMediaRegistryEntryStatus::EDeleted`

## 목록 UI 설계 초안

목록 화면은 registry entry를 직접 표시하고, 선택된 entry에서 manifest를 다시 열어 상세/edit/play로 들어가는 구조가 좋다.

### 최소 화면 구성

```text
WBP_RecordMediaListScreen
- Header
  - Back
  - Refresh
  - Upload/Sync 상태
- ListView_RegistryEntries
  - WBP_RecordMediaListItem
- Footer / DetailPanel
  - Play
  - Preview
  - Edit
  - Delete
  - Upload
```

### List item에 표시할 값

| UI 요소 | Registry field |
| --- | --- |
| 제목 | `published_display_name` 또는 `record_id` |
| 날짜 | `created_unix_time` |
| 해상도 | `width`, `height` |
| 길이 추정 | `recorded_frame_count / fps` |
| 용량 | `file_size_bytes` |
| 상태 badge | `registry_status`, `media_publish_status`, `preview_status` |
| preview image/video | `thumbnail_file_path`, `preview_clip_file_path`, `output_file_path`, `published_content_uri` |

### 목록 refresh 트리거

1. 화면 진입 시 `MetadataStore.RefreshRegistryFromDisk`.
2. 녹화 완료 delegate 이후 registry refresh.
3. 사용자가 Refresh 버튼 클릭.
4. 업로드/삭제/복원 작업 완료 후 registry refresh.

### 목록 UI 주의점

- Registry는 캐시이므로, 중요한 상세 정보는 선택 시 manifest를 다시 열어 확인한다.
- `output_file_exists=false`이지만 `published_content_uri`가 있으면 preview/play 가능성이 있다.
- `is_deleted=true`인 항목은 기본 목록에서 숨기되, 관리 화면에서는 표시할 수 있다.
- Upload/Sync 상태는 registry에 바로 넣기보다 AppState 또는 향후 server sync state와 분리하는 편이 안전하다.

## 스키마 변경 규칙

- v1 필드 이름은 가능한 한 유지한다.
- 필드 추가는 optional로 시작한다.
- 필드 삭제 대신 deprecated 처리한다.
- parser는 모르는 필드를 무시한다.
- enum string은 현재 `StaticEnum()->GetValueAsString` 결과를 기준으로 한다.
- 새 enum 값을 추가하면 이 문서의 값 목록도 갱신한다.
- manifest serializer/parser, registry serializer/parser, 이 문서를 같은 작업 단위로 갱신한다.

## 다음 확정 필요 사항

- `canView`, `canEdit`을 manifest 필드로 저장할지, runtime 평가 결과로만 둘지 결정.
- master/developer/local user id 발급 규칙.
- stream token과 edit token 분리 규칙.
- upload 완료 후 `playback.locator`를 remote url로 바꿀지, local/content uri를 계속 유지할지 결정.
- 삭제 정책: local delete, registry soft delete, server delete를 어떻게 구분할지 결정.
