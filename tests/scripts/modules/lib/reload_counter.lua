reload_module_loads = (reload_module_loads or 0) + 1

return {
    hits = 0,
    loads = reload_module_loads,
}
