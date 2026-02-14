
import sys
import json
import time
import argparse
from pathlib import Path
from PySide6.QtWidgets import QApplication, QWidget, QMessageBox
from PySide6.QtCore import Qt, QPoint, QRect
from PySide6.QtGui import QPainter, QPen, QColor, QPainterPath, QFont

class TrajectoryRecorder(QWidget):
    def __init__(self, initial_state=1):
        super().__init__()
        # 设置窗口为全屏、无边框、置顶、透明背景
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setWindowState(Qt.WindowFullScreen)
        self.setCursor(Qt.CrossCursor)
        self.setMouseTracking(True)  # 启用鼠标追踪，即使不按键也能看到当前位置
        
        # 数据存储
        self.current_path = []  # 存储当前的绘制路径 [(x, y, time_offset, state_id), ...]
        self.start_time = 0
        self.is_recording = False
        
        # 当前选中的状态（支持自定义）
        self.current_state_id = max(0, min(9, initial_state))  # 限制在 0-9 范围内
        
        # 自定义 GIF 映射表（可扩展）
        # 按键 0-9 对应的 GIF 文件名（相对于 characters/ 目录）
        self.gif_key_mapping = {
            1: "state1.gif",
            2: "state2.gif",
            3: "state3.gif",
            4: "state4.gif",
            5: "state5.gif",
            6: "state6.gif",
            7: "state7.gif",
            8: "aemeath.gif",  # 按 8 对应到 aemeath.gif
            9: "state1.gif",   # 可自定义
            0: "state1.gif",   # 可自定义
        }
        self.current_gif_name = self.gif_key_mapping.get(self.current_state_id, "state1.gif")
        
        # 视觉显示
        self.display_path = QPainterPath()
        self.state_change_points = [] # 记录哪里切换了状态，用于绘制不同颜色的点 [(pos, state_id)]

        print("【轨迹录制器已启动】")
        print("操作指南：")
        print("1. 按住鼠标左键：开始绘制轨迹")
        print("2. 绘制过程中按数字键 0-9：切换角色动画 GIF (1-7:state, 8:aemeath.gif)")
        print("3. 松开鼠标左键：结束当前绘制（可再次按住进行多段录制，形成‘断点’效果）")
        print("   -> 提示：若需从左边/右边出现，可先让鼠标移入边缘，然后开始绘制。")
        print("4. 按 'S' 键：保存完整轨迹到文件")
        print("5. 按 'R' 键：重新开始（清空所有数据）")
        print("6. 按 'Esc' 或 'Q' 键：退出")

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.is_recording = True
            
            # 如果是第一次开始录制（路径为空），记录开始时间
            if not self.current_path:
                self.start_time = time.time()
                self.display_path = QPainterPath()
                self.state_change_points = []
                self.display_path.moveTo(event.pos())
            else:
                # 如果已经有路径，说明是“断点”继续录制
                # 视觉上，我们把上一段的终点连到这一段的起点，形成一条路径线
                self.display_path.moveTo(event.pos())
            
            # 记录当前点
            self._record_point(event.pos())
            self.update()

    def mouseMoveEvent(self, event):
        if self.is_recording:
            # 记录当前点
            self._record_point(event.pos())
            
            # 手绘路径连线
            self.display_path.lineTo(event.pos())
            self.update()
        else:
            # 只是为了刷新界面显示鼠标旁边的文字（可选）
            self.update()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.is_recording = False
            self.update()

    def keyPressEvent(self, event):
        key = event.key()
        
        # 监听数字键 0-9
        if Qt.Key_0 <= key <= Qt.Key_9:
            num = key - Qt.Key_0
            
            # 检查映射表中是否有这个按键
            if num in self.gif_key_mapping:
                self.current_state_id = num
                self.current_gif_name = self.gif_key_mapping[num]
                print(f"-> 切换到按键 {num}: {self.current_gif_name}")
                
                if self.is_recording:
                    # 如果正在录制，记录这个切换点
                    current_pos = self.mapFromGlobal(self.cursor().pos())
                    self.state_change_points.append((current_pos, num))
                self.update()

        elif key == Qt.Key_S:
            self.save_trajectory()
        elif key == Qt.Key_R:
            self.current_path = []
            self.display_path = QPainterPath()
            self.state_change_points = []
            self.update()
            print("已重置")
        elif key in (Qt.Key_Escape, Qt.Key_Q):
            self.close()

    def _record_point(self, pos: QPoint):
        elapsed = time.time() - self.start_time
        self.current_path.append({
            "x": pos.x(),
            "y": pos.y(),
            "t": round(elapsed, 4), # 保留4位小数
            "s": self.current_state_id # 记录当前状态ID
        })

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # 绘制半透明背景
        painter.fillRect(self.rect(), QColor(0, 0, 0, 80))
        
        # 绘制路径线
        if not self.display_path.isEmpty():
            pen = QPen(QColor(0, 255, 255), 3) # 青色线条
            pen.setJoinStyle(Qt.RoundJoin)
            pen.setCapStyle(Qt.RoundCap)
            painter.setPen(pen)
            painter.drawPath(self.display_path)

            # 绘制起点
            start_pt = self.display_path.elementAt(0)
            painter.setBrush(QColor(0, 255, 0))
            painter.drawEllipse(QPoint(start_pt.x, start_pt.y), 6, 6)

            # 绘制状态切换点
            font = QFont()
            font.setBold(True)
            font.setPointSize(12)
            painter.setFont(font)
            
            for pt, state_id in self.state_change_points:
                painter.setBrush(QColor(255, 200, 0)) # 黄色点表示切换
                painter.drawEllipse(pt, 8, 8)
                painter.setPen(QColor(255, 255, 255))
                painter.drawText(pt.x() + 10, pt.y() - 10, f"S{state_id}")
                
            # 恢复画笔画路径
            painter.setPen(pen)

            if not self.is_recording:
                # 终点
                end_pt = self.display_path.currentPosition()
                painter.setBrush(QColor(255, 0, 0))
                painter.drawEllipse(end_pt, 6, 6)

        # 在左上角显示当前状态提示
        painter.setPen(QColor(255, 255, 255))
        painter.setFont(QFont("Microsoft YaHei", 14))
        status_text = f"当前 GIF: {self.current_gif_name} (按 0-9 切换)"
        if self.is_recording:
            status_text += " [录制中...]"
        else:
            status_text += " [等待录制] (按住左键画线)"
        painter.drawText(20, 40, status_text)
    
    def save_trajectory(self):
        if not self.current_path:
            print("没有路径可保存！")
            return
            
        timestamp = int(time.time())
        filename = f"trajectory_{timestamp}.json"
        
        # 存到项目根目录下的 recorded_paths 文件夹
        save_dir = Path("recorded_paths") 
        save_dir.mkdir(exist_ok=True)
        filepath = save_dir / filename
        
        data = {
            "total_points": len(self.current_path),
            "total_duration": self.current_path[-1]['t'],
            "points": self.current_path
        }
        
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
            print(f"✅ 成功保存轨迹到: {filepath.absolute()}")
            QMessageBox.information(self, "保存成功", f"文件已保存:\n{filepath.name}\n路径点: {len(self.current_path)}")
        except Exception as e:
            print(f"❌ 保存失败: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="轨迹录制器 - 录制角色动画轨迹",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  python trajectory_recorder.py              # 默认从按键 1 (state1.gif) 开始
  python trajectory_recorder.py --state 8    # 从按键 8 (aemeath.gif) 开始录制
  python trajectory_recorder.py -s 3         # 从按键 3 (state3.gif) 开始录制
  
按键映射:
  1-7: state1.gif ~ state7.gif
  8: aemeath.gif
  9,0: 自定义 (在代码中修改)
        """
    )
    parser.add_argument(
        '--state', '-s',
        type=int,
        default=1,
        choices=range(0, 10),
        metavar='0-9',
        help='初始 GIF 按键 (0-9)，默认为 1 (state1.gif), 8 对应 aemeath.gif'
    )
    
    args = parser.parse_args()
    
    app = QApplication(sys.argv)
    recorder = TrajectoryRecorder(initial_state=args.state)
    recorder.show()
    sys.exit(app.exec())

