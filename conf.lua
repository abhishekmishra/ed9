--- conf.lua: configuration for ed9
--
-- author: Abhishek Mishra
-- date: 02/09/2024

local canvasWidth = 640
local canvasHeight = 480

function love.conf(t)
    -- set the window title
    t.window.title = "ED9: Programmer's Editor"

    -- set the window size
    t.window.width = canvasWidth
    t.window.height = canvasHeight

    -- disable unused modules for performance
    t.modules.joystick = false
    t.modules.physics = false
    t.modules.touch = false

    -- enable console
    -- TODO: turning on console crashes Love2D on Windows,
    -- so it's disabled for now
    -- t.console = true
end
