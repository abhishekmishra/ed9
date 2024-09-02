--- main.lua - main file for the ed9 editor.
-- 
-- author: Abhishek Mishra
-- date: 02/09/2024

local cw, ch
local titleText = "ED9: Programmer's Editor"
local titleWidth, titleHeight

-- love.load - main entry point for Love2d
function love.load()
    cw, ch = love.graphics.getDimensions()
    titleWidth = love.graphics.getFont():getWidth(titleText)
    titleHeight = love.graphics.getFont():getHeight(titleText)
    love.graphics.setBackgroundColor(0.1, 0.1, 0.1)
end

-- love.draw - called every frame to draw the screen
function love.draw()
    love.graphics.print(titleText, cw/2 - titleWidth/2, ch/2 - titleHeight/2)
end
