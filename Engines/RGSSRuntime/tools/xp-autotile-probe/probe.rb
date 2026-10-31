# XP autotile probe: every quarter of a 96x128 autotile gets its own red value (5 * index), and the map shows the 48
# shapes in an 8x6 grid. A magenta square marks the screen's corner.
bmp = Bitmap.new(96, 128)
for qy in 0...8
  for qx in 0...6
    i = qy * 6 + qx
    bmp.fill_rect(qx * 16, qy * 16, 16, 16, Color.new(i * 5, 128, 64))
  end
end
marker = Sprite.new
marker.bitmap = Bitmap.new(4, 4)
marker.bitmap.fill_rect(0, 0, 4, 4, Color.new(255, 0, 255))
marker.z = 9999
tilemap = Tilemap.new
tilemap.tileset = Bitmap.new(256, 32)
tilemap.autotiles[0] = bmp
data = Table.new(20, 15, 3)
for s in 0...48
  data[1 + (s % 8) * 2, 1 + (s / 8) * 2, 0] = 48 + s
end
tilemap.map_data = data
tilemap.priorities = Table.new(384)
loop do
  Graphics.update
  tilemap.update
end
