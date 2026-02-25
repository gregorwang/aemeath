from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class CharacterStateInfo:
    """Single source of truth for character state metadata."""

    state_id: int
    gif_filename: str
    label: str
    description: str
    emotion: str | None = None


CHARACTER_STATES: dict[int, CharacterStateInfo] = {
    1: CharacterStateInfo(
        state_id=1,
        gif_filename="state1.gif",
        label="默认/好奇",
        description="角色默认待机，好奇探头",
        emotion="neutral",
    ),
    2: CharacterStateInfo(
        state_id=2,
        gif_filename="state2.gif",
        label="兴奋/活跃",
        description="角色兴奋互动或用户点击后的活跃状态",
    ),
    3: CharacterStateInfo(
        state_id=3,
        gif_filename="state3.gif",
        label="律动/音乐",
        description="角色跟随音乐律动或漫步时的状态",
    ),
    4: CharacterStateInfo(
        state_id=4,
        gif_filename="state4.gif",
        label="害羞/逃跑",
        description="角色害羞逃跑或忙碌时的状态",
        emotion="angry",
    ),
    5: CharacterStateInfo(
        state_id=5,
        gif_filename="state5.gif",
        label="思考/观察",
        description="角色思考、观察、鼠标悬停或屏幕评论时的状态",
        emotion="sad",
    ),
    6: CharacterStateInfo(
        state_id=6,
        gif_filename="state6.gif",
        label="开心/问候",
        description="角色开心打招呼、被召唤或用户回来时的状态",
        emotion="happy",
    ),
    7: CharacterStateInfo(
        state_id=7,
        gif_filename="state7.gif",
        label="环境/装饰",
        description="角色作为微型装饰或探头侦查时的状态",
    ),
    8: CharacterStateInfo(
        state_id=8,
        gif_filename="aemeath.gif",
        label="主角色",
        description="主角色标准动画",
    ),
}


def get_state_label(state_id: int) -> str:
    info = CHARACTER_STATES.get(state_id)
    return info.label if info else f"未知状态 {state_id}"


def get_gif_filename(state_id: int) -> str:
    info = CHARACTER_STATES.get(state_id)
    return info.gif_filename if info else f"state{state_id}.gif"


def build_expression_state_map() -> dict[str, str]:
    mapping: dict[str, str] = {}
    for info in CHARACTER_STATES.values():
        if info.emotion:
            mapping[info.emotion.strip().lower()] = f"state{info.state_id}"
    return mapping
