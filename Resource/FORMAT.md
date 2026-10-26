# Resource 포맷 명세

> 로더는 `SimpleGame/` 에 들어가고 이 채팅 범위가 아니다. 그래서 **로더가 알아야 할 모든 약속을
> 여기에 적어 둔다.** 나중에 코드를 쓸 때 이 문서만 보면 되도록 하는 것이 목적이다.

---

## 1. 내보내기 포맷 — glTF 2.0 (`.gltf` + `.bin`)

| 후보 | 채택 여부 | 이유 |
|---|---|---|
| **glTF 2.0 (separate)** | **채택** | 스켈레톤·스키닝·애니메이션을 한 포맷이 다 담는다. `.gltf` 가 JSON 이라 눈으로 열어 확인할 수 있고, 버텍스 버퍼는 `.bin` 으로 빠져 파싱이 단순하다. 로더를 직접 짜기에 가장 만만하다. |
| OBJ | 탈락 | **스켈레톤도 애니메이션도 못 담는다.** 게다가 `.gitignore:78` 의 `*.obj` (C++ 오브젝트 파일 규칙) 에 걸려 커밋되지 않는다. |
| FBX | 탈락 | 바이너리 독점 포맷. 파서를 직접 짜는 건 과제 범위를 넘는다. |
| GLB (단일 바이너리) | 보류 | 배포에는 낫지만 디버깅 때 안을 못 본다. 필요하면 `tools/common/export.py` 의 `GLTF_FORMAT` 한 줄만 바꾸면 된다. |

**버퍼는 전부 `.bin` 하나에 모인다** (`GLTF_SEPARATE`). base64 임베드는 쓰지 않는다 — 파일이 3배로 붓고
파싱 때 디코딩 단계가 하나 더 붙는다.

### 로더가 지원해야 하는 glTF 기능 범위

생성 스크립트는 아래만 쓴다. 이 밖의 것은 나오지 않으니 로더도 구현할 필요가 없다.

- `meshes` — `POSITION`, `NORMAL`, `COLOR_0`, `JOINTS_0`, `WEIGHTS_0`. **UV 와 텍스처는 쓰지 않는다** (3절 참고)
- 프리미티브 모드는 `4` (TRIANGLES) 뿐. 인덱스는 `UNSIGNED_SHORT` 또는 `UNSIGNED_INT`
- `skins` — `inverseBindMatrices`, `joints`, `skeleton`
- `animations` — `translation` / `rotation` / `scale` 채널, 보간은 **`LINEAR`** (`CUBICSPLINE` 안 씀)
- `materials` — `pbrMetallicRoughness.baseColorFactor`, `metallicFactor`, `roughnessFactor`, `emissiveFactor`
- 스파스 액세서, 모프 타깃, 카메라, 라이트 — **전부 안 씀**

#### 예외 둘 — 실제로 구워 보고 확인한 것

**(1) `base` 클립만 `STEP` 보간으로 나간다.** 값이 전 구간 같은 상수 곡선이라
내보내기가 "보간할 것이 없다" 고 판단한다. **LINEAR 로 읽어도 결과가 같으므로**
로더는 STEP 을 따로 구현하지 않아도 된다. 다만 값이 *변하는데* STEP 인 곡선은
동작이 끊긴다는 뜻이니, `tools/verify_export.py` 가 그 경우만 골라 실패로 잡는다.

**(2) `KHR_materials_emissive_strength` 확장이 하나 붙는다.** 등불·이계 발광처럼
발광 세기가 1.0 을 넘는 머티리얼 때문이다. glTF 의 `emissiveFactor` 는 0~1 이라
세기를 따로 실을 곳이 없다.

- `extensionsRequired` 가 **아니다.** 무시해도 파일을 읽을 수 있다.
- 무시하면 정규화된 `emissiveFactor` 만 쓰게 되어 **조금 덜 밝을 뿐** 색은 맞다.
- 세기 값이 필요하면 확장을 파싱할 것 없이 `data/nature_catalog.json` 의
  `materials.<이름>.emissive` 를 읽으면 된다.

이 둘 말고 다른 확장이나 보간이 나오면 `verify_export.py` 가 실패로 잡는다.

---

## 2. 좌표계 · 단위 · 기준점

| 항목 | 값 |
|---|---|
| 단위 | **1 unit = 1 meter** |
| Blender 내부 축 | Z-up, 캐릭터는 **-Y 방향을 바라본다** (Blender 관례) |
| glTF 출력 축 | **Y-up, 전방 +Z** (`export_yup=True`, glTF 표준) |
| 원점 | 발 사이 **바닥 중앙** = `(0, 0, 0)`. 소품은 바닥 접지면 중앙 |
| 회전 단위 | 쿼터니언 (glTF `rotation` 은 `[x, y, z, w]`) |

> **주의 — 지금 게임의 깊이 규약과 충돌한다.** `CLAUDE.md` 4-1 의 "깊이 = `x + y`, `z` 는 화면 Y만 민다"
> 는 2D 스프라이트 전제다. 3D 메쉬를 쓰면 실제 깊이 버퍼가 필요하고 그 규약은 더 이상 안 맞는다.
> 이 폴더는 **리소스만** 책임진다. 렌더러를 어느 쪽으로 가져갈지는 코드 작업에서 정할 일이다.

### 축 변환 요약 (로더용)

glTF 를 그대로 읽으면 **Y-up / 전방 +Z / 오른손 좌표계**다. 게임이 Z-up 이라면 로드 시
`(x, y, z)_gltf → (x, -z, y)_game` 한 번만 돌리면 된다.

---

## 3. 머티리얼 — 텍스처 없음, 버텍스 컬러 기반

로우폴리 + 레고 컨셉이라 **UV 맵도 텍스처도 만들지 않는다.** 색은 두 경로로 들어간다.

1. **머티리얼 `baseColorFactor`** — 면 단위 고정색. 파츠 하나가 보통 1~3개 머티리얼을 쓴다.
2. **`COLOR_0` 버텍스 컬러** — 같은 메쉬 안에서 색을 흔들 때 (나뭇잎, 바위 얼룩, 땅 타일).

덕분에 로더에 **이미지 디코더가 필요 없다.** 텍스처 로딩·밉맵·아틀라스 전부 생략된다.

### `COLOR_0` 은 색이 아니라 **배율**이다

```
최종색 = baseColorFactor × COLOR_0
```

glTF 규약이 그렇게 곱하도록 정해 두었다. 그래서 정점에 절대색을 넣으면 **색이 두 번
곱해져 거의 검게 죽는다.** 생성 스크립트는 굽는 시점에 머티리얼 색으로 나눠 배율로
바꿔서 넣는다 (`common/build.py` 의 `MeshBuilder.bake`).

**배율은 1.0 을 넘지 못한다.** 내보낼 때 정규화 정수(`UNSIGNED_SHORT`)가 되기 때문이다.
그래서 변주는 **어두워지는 쪽으로만** 준다 — 머티리얼 색이 "가장 밝은 기준" 이고
정점 색은 거기서 깎는 역할만 한다. 밝게 칠하려 했다면 머티리얼 쪽을 올려야 한다.
`tools/check.py` 가 기준보다 밝은 정점 색을 굽기 전에 잡아 준다.

`COLOR_0` 이 없는 프리미티브는 1.0 으로 친다 (glTF 기본값). 캐릭터·짐승·소품에는
정점 색이 없고, 자연물에만 있다.

> **내보내기 설정 주의.** Blender 5.2 는 기본값으로 같은 색 속성을 `COLOR_0`(ubyte) 과
> `COLOR_1`(ushort) **두 벌**로 내보낸다. 반대로 `export_vertex_color='MATERIAL'` 로
> 두면 머티리얼 노드가 정점 색을 참조하지 않는 한 아예 빠진다 (우리는 노드를 쓰지 않고
> 팩터만 쓰므로 늘 빠진다). `'NAME'` 모드로 속성 이름을 콕 집어야 `COLOR_0` 하나가 나간다.
> `common/export.py` 에 그렇게 박아 두었다.

### 색 슬롯 (파츠 색 교체)

지금 코드의 `ColorSlot` (`SimpleGame/Model.h`) 과 같은 발상이다. 머티리얼 **이름**으로 슬롯을 표시한다.

| 머티리얼 이름 | 뜻 | 인스턴스가 덮어쓰나 |
|---|---|---|
| `slot.skin` | 피부 | O |
| `slot.hair` | 머리카락 | O |
| `slot.cloth_main` | 겉옷 주색 | O |
| `slot.cloth_trim` | 옷 배색·테두리 | O |
| `slot.leather` | 가죽·벨트·신발 | O |
| `slot.metal` | 금속 | O |
| `slot.fur_main` | 짐승 몸통 털 | O |
| `slot.fur_belly` | 짐승 배·아래쪽 | O |
| `slot.horn` | 뿔·발굽·이빨 | O |
| `slot.eye` | 눈 | O |
| `fix.*` | 고정색 (교체 안 함) | X |
| `emit.eldritch` | 이계(異界) 발광 — 오염 개체 | X |

로더는 `slot.` 으로 시작하는 머티리얼의 `baseColorFactor` 를 인스턴스 색으로 갈아끼우면 된다.
`baseColorFactor` 에 들어 있는 값은 **기본값이자 폴백**이다.

---

## 4. 네이밍 규칙

```
<카테고리>_<그룹>_<이름>[_<변형>]
```

| 대상 | 예 |
|---|---|
| 캐릭터 파츠 메쉬 | `chr_head_round`, `chr_torso_tunic`, `chr_legs_trouser` |
| 짐승 메쉬 | `cre_canine_wolf_body`, `cre_cervid_stag_antler` |
| 소품 | `prop_house_wall_straight`, `prop_tool_bucket` |
| 자연물 | `nat_tree_pine`, `nat_rock_boulder` |
| 본 | `hips`, `spine_01`, `upperarm_L` — **좌우는 접미사 `_L` / `_R` 대문자** |
| 소켓 본 | `sk_hand_R`, `sk_back` — 손에 쥐는 물건이 붙는 자리 |
| 애니메이션 클립 | `walk`, `run`, `turn_L`, `attack_01`, `jump_start` |
| 머티리얼 | `slot.skin`, `fix.wood_dark`, `emit.eldritch` |

**규칙:** 전부 소문자 + 언더스코어. 공백·한글·대시 금지 (좌우 접미사만 예외).
`.gltf` 안의 노드 이름이 그대로 이 이름이므로 **로더가 문자열로 찾을 수 있다.**

---

## 5. 캐릭터 조립 규약 (레고)

### 핵심 — 파츠를 갈아끼워도 릭은 하나다

모든 캐릭터 파츠는 **같은 휴머노이드 아마튜어에 스키닝**되어 있다.
그래서 "조립"은 본을 잇는 작업이 아니라 **어느 슬롯에 어느 메쉬를 켜느냐**의 문제다.

```
캐릭터 1명 = 아마튜어 1개 + 슬롯별 메쉬 N개 (전부 같은 아마튜어에 바인딩)
```

파츠를 바꿔도 **스킨 웨이트가 이미 맞춰져 있으므로 애니메이션이 그대로 돈다.**
이게 파츠마다 릭을 따로 두는 방식 대비 가장 큰 이득이다.

### 슬롯 목록

| 슬롯 | 필수 | 비고 |
|---|---|---|
| `head` | O | 두개골 형태 |
| `face` | O | 눈·코·입 — 머리와 분리해 표정 변주 |
| `hair` | · | 없을 수 있다 (대머리·투구) |
| `torso` | O | 상체 옷 |
| `arms` | O | 양팔 한 덩이 |
| `hands` | O | |
| `legs` | O | |
| `feet` | O | |
| `cloak` | · | 망토 |
| `belt` | · | |
| `headgear` | · | 모자·투구 — `hair` 와 배타 가능 |
| `backpack` | · | |

조합 가능한 파츠 목록은 [`data/character_parts.json`](data/character_parts.json) 에 있다.

### 소켓 — 손에 드는 물건

소켓은 **메쉬가 없는 본**이다. 무기·등불 같은 걸 붙일 좌표만 제공한다.

| 소켓 본 | 자리 |
|---|---|
| `sk_hand_R` | 오른손 손바닥. 주무기 |
| `sk_hand_L` | 왼손 손바닥. 등불·방패 |
| `sk_back` | 등 중앙. 수납한 무기 |
| `sk_hip_L` | 왼쪽 허리. 칼집 |
| `sk_head_top` | 정수리. 모자 보정용 |

로더는 소켓 본의 월드 행렬을 가져와 거기에 소품 메쉬를 곱해 그리면 된다.

---

## 6. 스켈레톤

### 6-1. 휴머노이드 (`rig_humanoid`) — 7등신

머리 하나 = `H = 0.25 m`, 전체 키 `7H = 1.75 m`.

```
root
└ hips
  ├ spine_01 → spine_02 → chest
  │ ├ neck → head → (sk_head_top)
  │ ├ shoulder_L → upperarm_L → lowerarm_L → hand_L → (sk_hand_L)
  │ └ shoulder_R → upperarm_R → lowerarm_R → hand_R → (sk_hand_R)
  ├ thigh_L → shin_L → foot_L → toe_L
  ├ thigh_R → shin_R → foot_R → toe_R
  ├ (sk_back)
  └ (sk_hip_L)
```

높이 기준점 (바닥에서, 전체 키 `T = 1.75` 비율):

| 지점 | 비율 | 미터 |
|---|---|---|
| 발목 | 0.039 T | 0.068 |
| 무릎 | 0.285 T | 0.499 |
| 가랑이 | 0.500 T | 0.875 |
| 허리 | 0.620 T | 1.085 |
| 가슴 | 0.720 T | 1.260 |
| 어깨 | 0.820 T | 1.435 |
| 턱 | 0.857 T | 1.500 |
| 정수리 | 1.000 T | 1.750 |

`root` 는 원점에 두고 움직이지 않는다 — **루트 모션은 쓰지 않는다.**
이동은 게임 코드가 하고, 애니메이션은 제자리에서 돈다 (6-3 참고).

### 6-2. 4족 (`rig_quadruped`) — 뒷다리가 핵심

```
root
└ hips
  ├ spine_03 → spine_02 → spine_01 → chest       (엉덩이에서 앞으로 간다)
  │ ├ neck_01 → neck_02 → head → jaw
  │ ├ scapula_L → upperarm_L → lowerarm_L → frontfoot_L → fronttoe_L
  │ └ scapula_R → ... (대칭)
  ├ thigh_L → shin_L → hock_L → metatarsus_L → hindtoe_L
  ├ thigh_R → ... (대칭)
  └ tail_01 → tail_02 → tail_03
```

**뒷다리 5단 구조가 이 릭의 요점이다.** 사람 다리를 네 다리에 그대로 쓰면
"역관절처럼 뒤로 꺾이는" 짐승 뒷다리 느낌이 전혀 안 난다.

| 본 | 해부학 | 역할 |
|---|---|---|
| `thigh` | 대퇴골 | 짧고 두껍다. 엉덩이 근육에 거의 묻혀 있다 |
| `shin` | 경골 | 길다. 앞으로 기울어진다 |
| `hock` | 발꿈치(비절) | **뒤로 꺾이는 그 관절.** 지면에서 떠 있다 |
| `metatarsus` | 중족골 | 사람으로 치면 발등. 짐승은 이걸 세워서 딛는다 |
| `hindtoe` | 발가락/발굽 | 실제 접지면 |

과(科)별 차이는 **뼈 길이 비율**로 만든다 (`tools/common/rig_quadruped.py` 의 `FAMILY`):

| 과 | 보행형 | 특징 |
|---|---|---|
| 고양이과 (`feline`) | 지행 (digitigrade) | 긴 몸통, 유연한 척추, 어깨뼈가 솟는다, 긴 꼬리, 발바닥 접지 |
| 개과 (`canine`) | 지행 (digitigrade) | 깊은 흉곽, 곧은 등, 중간 길이 다리, 꼬리 중간 |
| 사슴과 (`cervid`) | 제행 (unguligrade) | **중족골이 아주 길다**, 다리 전체가 가늘고 길다, 발굽 접지, 긴 목, 짧은 꼬리 |

### 6-3. 루트 모션 없음

모든 이동 클립은 **제자리(in-place)** 다. `root` 본은 전 프레임 원점 고정.
대신 각 클립의 "원래 이동 속도"를 [`data/anim_graph.json`](data/anim_graph.json) 의 `speed_mps` 에 적어 둔다.
게임은 그 값으로 재생 속도를 맞춘다 (7-3 발미끄러짐 항목).

---

## 7. 애니메이션

### 7-1. 클립 목록

종류마다 아래를 전부 만든다.

| 클립 | 루프 | 길이(s) | 용도 |
|---|---|---|---|
| `base` | — | 1프레임 | **바인드 포즈.** 모든 보간의 기준이자 폴백 |
| `idle` | O | 2.40 | 대기. 호흡만 |
| `walk` | O | 1.00 | 걷기 |
| `run` | O | 0.60 | 뛰기 |
| `turn_L` | X | 0.50 | 제자리 좌회전 |
| `turn_R` | X | 0.50 | 제자리 우회전 |
| `attack_01` | X | 0.70 | 공격 |
| `jump_start` | X | 0.25 | 도약 — 웅크렸다 뜨기 |
| `jump_air` | O | 0.60 | 체공 (루프) |
| `jump_land` | X | 0.35 | 착지 |

> **점프를 셋으로 쪼갠 이유:** 체공 시간은 게임이 결정한다 (높이·중력·지형).
> 점프를 한 클립으로 두면 길이가 고정되어 실제 체공과 어긋난다.
> `start → air(루프) → land` 로 끊으면 체공만 늘였다 줄였다 할 수 있다.

### 7-2. `base` 포즈 — 회전 보간의 기준

`turn_L` / `turn_R` 은 **`base` 와 같은 포즈에서 시작해 같은 포즈로 끝난다.**
그래서 회전 도중 다른 상태로 끊겨도 포즈가 튀지 않는다.
`base` 는 단순한 바인드 포즈가 아니라 **모든 클립의 0프레임이 공유하는 기준 포즈**다.

### 7-3. 블렌드 / 전이 정보

"걷다가 뛸 때 필요한 조합 정보" 는 [`data/anim_graph.json`](data/anim_graph.json) 에 들어 있다. 세 가지다.

**(1) 1D 속도 블렌드 트리** — `idle ↔ walk ↔ run`

```json
"locomotion": {
  "parameter": "speed_mps",
  "stops": [
    { "clip": "idle", "value": 0.0 },
    { "clip": "walk", "value": 1.35 },
    { "clip": "run",  "value": 4.20 }
  ]
}
```

속도가 2.0 m/s 면 `walk` 와 `run` 을 `(2.0-1.35)/(4.20-1.35) = 0.228` 로 섞는다.

**(2) 접지 위상 (`contact_phase`) — 발미끄러짐 방지의 핵심**

두 보행 클립을 그냥 섞으면 발이 미끄러진다. 한쪽은 왼발이 땅에 닿는 순간인데
다른 쪽은 공중이면, 섞인 결과는 **어느 쪽도 땅에 안 붙은** 포즈가 되기 때문이다.

그래서 모든 보행 클립의 **왼발 접지 순간을 위상 0.0 에 맞춰 두었다.**

```json
"walk": { "contact_phase": { "foot_L": 0.00, "foot_R": 0.50 } },
"run":  { "contact_phase": { "foot_L": 0.00, "foot_R": 0.50 } }
```

재생할 때 두 클립의 **정규화 시간(0~1)을 공유**하면 발이 맞는다.
`walk` 가 1.0초, `run` 이 0.6초로 길이가 달라도 **위상으로 돌리면** 문제없다.

```
phase += dt / lerp(walk.duration, run.duration, blend);   // 공유 위상
phase  = fmod(phase, 1.0);
pose   = lerp( sample(walk, phase), sample(run, phase), blend );
```

4족은 발이 넷이라 `foot_FL / foot_FR / foot_HL / foot_HR` 네 개를 적어 둔다.
걸음걸이(gait)마다 순서가 다르다 — `walk` 는 대각선 교대, `run` 은 포유류 질주(앞발 모았다 뒷발 모으기).

**(3) 전이 블렌드 시간 (`transitions`)**

```json
{ "from": "walk", "to": "run", "duration": 0.18, "sync": "phase" }
{ "from": "run",  "to": "jump_start", "duration": 0.06, "sync": "none" }
```

- `sync: "phase"` — 위상을 이어받는다 (보행 ↔ 보행)
- `sync: "none"` — 그냥 크로스페이드 (보행 → 공격 등)
- 공격·점프처럼 **끼어드는 동작은 전이가 짧다** (0.05~0.10). 길면 반응이 굼떠 보인다

---

## 8. 폴리곤 예산

로우폴리 기준. 넘으면 리토폴로지 대상이다.

| 대상 | 삼각형 |
|---|---|
| 캐릭터 파츠 1개 | 80 ~ 400 |
| 캐릭터 1명 (전체 조립) | ~ 1,800 |
| 4족 짐승 1마리 | ~ 1,200 |
| 소품 (작은 집기) | 40 ~ 200 |
| 건물 모듈 1개 | 150 ~ 600 |
| 나무 1그루 | 200 ~ 500 |

**셰이딩은 플랫(flat) 기본.** 로우폴리 면이 각지게 보여야 레고 느낌이 산다.
둥글려야 하는 곳(머리·열매)만 스무스 셰이딩을 켠다.

---

## 9. 검증

두 스크립트 모두 **Blender 없이** 돈다.

| 명령 | 보는 것 |
|---|---|
| `python tools/check.py` | **굽기 전** 정의 — 좌표, 본 이름 오타, 폴리곤 예산, 4족 보행 순서, 정점 색이 머티리얼보다 밝지 않은지 |
| `python tools/verify_export.py` | **구워진 결과** — 위 1절의 기능 범위를 벗어나지 않았는지, 클립 길이가 문서와 맞는지, `baseColorFactor × COLOR_0` 이 팔레트대로 복원되는지 |

둘은 보는 것이 다르다. 정의가 맞아도 내보내기 설정 하나로 결과가 틀어질 수 있고,
실제로 그랬다 — 정점 색이 두 벌로 나가거나, 통째로 빠지거나 했다.

---

## 10. `.gitignore` 주의

- `*.obj` 가 이미 ignore 되어 있다 (`.gitignore:78`). **OBJ 는 애초에 안 쓰지만**, 혹시 중간
  산출물로 만들더라도 커밋되지 않는다는 걸 알고 있어야 한다.
- `.gltf` · `.bin` · `.blend` · `.json` 은 확인 결과 전부 추적된다.
- `blend/` 의 `.blend1` 백업 파일은 커밋하지 않는 편이 낫다.
