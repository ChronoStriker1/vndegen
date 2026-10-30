# Win32API, which RGSS games use to call Windows directly. Only the calls VNDegen's test and library games make are
# emulated; the rest log once and return 0, which games read as "nothing happened". docs/rgss.md lists them.
class Win32API
  # The game's window, as far as the game can tell.
  WINDOW = 0x10001

  # Windows virtual-key codes to the libretro key codes the runtime reads.
  KEYS = {
    0x08 => [8], 0x09 => [9], 0x0D => [13, 271], 0x10 => [304, 303], 0x11 => [306, 305], 0x12 => [308, 307],
    0x1B => [27], 0x20 => [32], 0x21 => [280], 0x22 => [281], 0x23 => [279], 0x24 => [278],
    0x25 => [276], 0x26 => [273], 0x27 => [275], 0x28 => [274], 0x2D => [277], 0x2E => [127],
    0xA0 => [304], 0xA1 => [303], 0xA2 => [306], 0xA3 => [305], 0xA4 => [308], 0xA5 => [307],
  }
  (0x30..0x39).each { |vk| KEYS[vk] = [vk] }
  (0x41..0x5A).each { |vk| KEYS[vk] = [vk + 32] }
  (0x60..0x69).each { |vk| KEYS[vk] = [256 + vk - 0x60] }
  (0x70..0x7B).each { |vk| KEYS[vk] = [282 + vk - 0x70] }
  MOUSE = { 0x01 => 0, 0x02 => 1, 0x04 => 2 }

  @warned = {}
  @profile = {}

  class << self
    attr_reader :profile

    def warn_once(name)
      return if @warned[name]
      @warned[name] = true
      Kernel.rgss_log("Win32API: #{name} is not emulated")
    end
  end

  def initialize(dll, function, import = '', export = '')
    @dll = dll.to_s.downcase.sub(/\.dll\z/, '')
    # The A and W forms behave alike here.
    @function = function.to_s.sub(/[AW]\z/, '')
  end

  def call(*args)
    args = args.flatten
    handler = "w32_#{@function}"
    return send(handler, *args) if respond_to?(handler, true)
    Win32API.warn_once("#{@dll}.#{@function}")
    0
  end
  alias Call call

  private

  def screen_size = [Graphics.width, Graphics.height]

  def put(buffer, bytes)
    return unless buffer.is_a?(String)
    buffer[0, bytes.bytesize] = bytes.b.force_encoding(buffer.encoding)
  end

  def rect(buffer, left, top, right, bottom) = put(buffer, [left, top, right, bottom].pack('l4'))

  # Window management: one window, the size of the screen, which never moves.
  def w32_FindWindow(*) = WINDOW
  def w32_FindWindowEx(*) = WINDOW
  def w32_GetActiveWindow = WINDOW
  def w32_GetForegroundWindow = WINDOW
  def w32_SetForegroundWindow(*) = 1
  def w32_GetSystemMetrics(index)
    width, height = screen_size
    { 0 => width, 1 => height, 4 => 0, 5 => 1, 6 => 1, 7 => 3, 8 => 3, 32 => 4, 33 => 4 }.fetch(index, 0)
  end
  def w32_SystemParametersInfo(action, _param, buffer, _flags)
    width, height = screen_size
    rect(buffer, 0, 0, width, height) if action == 0x30 # SPI_GETWORKAREA
    1
  end
  def w32_GetWindowRect(_window, buffer)
    width, height = screen_size
    rect(buffer, 0, 0, width, height)
    1
  end
  def w32_GetClientRect(_window, buffer) = w32_GetWindowRect(_window, buffer)
  %w[MoveWindow SetWindowPos ShowWindow UpdateWindow SetWindowLong SetWindowText ShowCursor ReleaseDC FillRect
     DeleteObject ClientToScreen ScreenToClient SetCursorPos].each { |name| define_method("w32_#{name}") { |*| 1 } }
  def w32_GetWindowLong(*) = 0
  def w32_GetDC(*) = 1
  def w32_CreateSolidBrush(*) = 1
  def w32_CreateWindowEx(*) = 0
  def w32_SendInput(count, *) = count
  def w32_keybd_event(*) = 0
  def w32_AllocConsole = 0
  def w32_SetConsoleTitle(*) = 0
  def w32_GetWindowTextLength(*) = window_title.bytesize
  def w32_GetWindowText(_window, buffer, size)
    title = window_title.byteslice(0, [size.to_i - 1, 0].max)
    put(buffer, title + "\0")
    title.bytesize
  end

  def window_title
    title = (Win32API.profile['game'] ||= {})['title']
    title || 'RGSS Player'
  end

  # Keys: bit 15 says the key is down.
  def key_down?(vk)
    return Input.rgss_mouse_down?(MOUSE[vk]) if MOUSE.key?(vk)
    (KEYS[vk] || []).any? { |code| Input.rgss_key_down?(code) }
  end
  def w32_GetAsyncKeyState(vk) = key_down?(vk) ? -32_768 : 0
  def w32_GetKeyState(vk) = key_down?(vk) ? -127 : 0

  # Game.ini settings. Reads come from the game's file; writes last until the game ends.
  def w32_GetPrivateProfileString(section, key, default, buffer, size, file)
    value = profile_value(section, key, file) || default.to_s
    value = value.byteslice(0, [size.to_i - 1, 0].max)
    put(buffer, value + "\0")
    value.bytesize
  end
  def w32_GetPrivateProfileInt(section, key, default, file)
    (profile_value(section, key, file) || default).to_i
  end
  def w32_WritePrivateProfileString(section, key, value, file)
    settings = (Win32API.profile[File.basename(file.to_s).downcase] ||= {})
    settings["#{section.to_s.downcase}/#{key.to_s.downcase}"] = value.to_s
    1
  end

  def profile_value(section, key, file)
    name = File.basename(file.to_s).downcase
    written = Win32API.profile.dig(name, "#{section.to_s.downcase}/#{key.to_s.downcase}")
    return written if written
    data = begin
      Kernel.rgss_read_game_file(File.basename(file.to_s))
    rescue StandardError
      nil
    end
    return nil unless data
    current = nil
    data.each_line do |line|
      line = line.strip
      if line =~ /\A\[(.*)\]\z/
        current = $1.downcase
      elsif current == section.to_s.downcase && line =~ /\A([^=]+)=(.*)\z/ && $1.strip.casecmp?(key.to_s)
        return $2.strip
      end
    end
    nil
  end

  # Text conversion between Windows code pages and UTF-16, which games use to measure or convert Japanese text.
  def code_page(page) = { 0 => 'Windows-31J', 932 => 'Windows-31J', 65_001 => 'UTF-8', 1252 => 'Windows-1252' }.fetch(page.to_i, 'Windows-31J')

  def w32_MultiByteToWideChar(page, _flags, source, length, buffer, size)
    bytes = source.to_s.b
    bytes = bytes.byteslice(0, length) if length.to_i >= 0
    wide = bytes.force_encoding(code_page(page)).encode('UTF-16LE', invalid: :replace, undef: :replace)
    count = wide.bytesize / 2
    return count if size.to_i == 0
    put(buffer, wide.byteslice(0, size.to_i * 2))
    [count, size.to_i].min
  end

  def w32_WideCharToMultiByte(page, _flags, source, length, buffer, size, *)
    wide = source.to_s.b
    wide = wide.byteslice(0, length.to_i * 2) if length.to_i >= 0
    text = wide.force_encoding('UTF-16LE').encode(code_page(page), invalid: :replace, undef: :replace).b
    return text.bytesize if size.to_i == 0
    put(buffer, text.byteslice(0, size.to_i))
    [text.bytesize, size.to_i].min
  end

  def w32_lstrlen(source) = source.to_s.b.index("\0") || source.to_s.bytesize
  def w32_GetUserDefaultLangID = 0x0411
  def w32_GetSystemDefaultLangID = 0x0411
  def w32_GetUserDefaultLCID = 0x0411
  def w32_GetTickCount = (Process.clock_gettime(Process::CLOCK_MONOTONIC) * 1000).to_i & 0x7FFF_FFFF
  def w32_timeGetTime = w32_GetTickCount
end
