-- Safety-net tab for the prototype lab.
function DrawPrototypeSafetyPanel()
    UI.TextDisabled("PROJECT MEMORY + SAFETY NET")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("Use these only when needed. They verify exact Lua bindings and native object lifetime rules without mixing the controls into the driving tabs.")
    UI.Spacing()
    DrawSafetyNetPanel()
end
