from __future__ import annotations

from enum import Enum, auto
from typing import Callable

from PySide6.QtCore import QObject, Signal


class EntityState(Enum):
    """Companion behavior states."""

    HIDDEN = auto()
    PEEKING = auto()
    ENGAGED = auto()
    FLEEING = auto()


class StateMachine(QObject):
    """
    Finite-state machine controller.

    Manages legal transitions and enter/exit callbacks.
    """

    state_changed = Signal(object, object)

    VALID_TRANSITIONS: dict[EntityState, list[EntityState]] = {
        EntityState.HIDDEN: [EntityState.PEEKING, EntityState.ENGAGED],
        EntityState.PEEKING: [EntityState.ENGAGED, EntityState.FLEEING, EntityState.HIDDEN],
        EntityState.ENGAGED: [EntityState.FLEEING, EntityState.HIDDEN],
        EntityState.FLEEING: [EntityState.HIDDEN],
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self._current_state = EntityState.HIDDEN
        self._callbacks: dict[EntityState, dict[str, Callable | None]] = {}

    @property
    def current_state(self) -> EntityState:
        return self._current_state

    def register_state_handler(
        self,
        state: EntityState,
        *,
        on_enter: Callable | None = None,
        on_exit: Callable | None = None,
    ) -> None:
        self._callbacks[state] = {"enter": on_enter, "exit": on_exit}

    def transition_to(self, new_state: EntityState) -> bool:
        if new_state == self._current_state:
            return True
        allowed = self.VALID_TRANSITIONS.get(self._current_state, [])
        if new_state not in allowed:
            print(f"[FSM] 非法状态转换: {self._current_state.name} -> {new_state.name}")
            return False

        old_state = self._current_state
        old_callbacks = self._callbacks.get(old_state, {})
        exit_fn = old_callbacks.get("exit")
        if callable(exit_fn):
            exit_fn()

        self._current_state = new_state
        new_callbacks = self._callbacks.get(new_state, {})
        enter_fn = new_callbacks.get("enter")
        if callable(enter_fn):
            enter_fn()

        self.state_changed.emit(old_state, new_state)
        print(f"[FSM] 状态转换: {old_state.name} -> {new_state.name}")
        return True

