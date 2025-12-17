"""
Genesis 离线叙事可观测性：Lenses（可组合观测算子）

约束：
- 只依赖 Python 标准库（便于在 CI/本地直接运行）
- 不在 runtime eventline 中做推断；推断/派生只允许发生在离线 lens 中
"""

