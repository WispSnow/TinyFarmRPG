module_counter_loads = (module_counter_loads or 0) + 1

return {
    name = "counter",
    loads = module_counter_loads,
}
