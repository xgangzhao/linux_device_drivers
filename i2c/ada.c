// i2c adapter driver

struct platform_device_id xxx_i2c_devtype[] = {
    // todo
    {},
    {},
};

MODULE_DEVICE_TABLE(platform, xxx_i2c_devtype);

struct of_device_id i2c_xxx_dt_ids[] = {
    // todo, .compatible, .data, ...
    {},
    {},
};

MODULE_DEVICE_TABLE(of, i2c_xxx_dt_ids);

/* core */
int i2c_xxx_probe(struct platform_device* pdev) {
    // match dev from oftable
    struct of_device_id* of_id = of_match_device(i2c_xxx_dt_ids, &pdev->dev);

    // get irq?
    // get resource?
    // get
}
struct platform_driver i2c_xxx_driver = {
    .probe = i2c_xxx_probe,
    .remove = i2c_xxx_remove,
    .driver = {
        .of_match_table = i2c_xxx_dt_ids,
    },
    .id_table = xxx_i2c_devtype,
};

int __init i2c_adap_xxx_init(void) {
    return platform_driver_register(&i2c_xxx_driver);
}

subsys_initcall(i2c_adap_xxx_init);

void __exit i2c_adap_xxx_exit(void) {
    return platform_driver_unregister(&i2c_xxx_driver);
}

int i2c_xxx_probe(struct platform_device* pdev) {
    struct of_device_id* of_id = of_match_device(i2c_xxx_dt_ids, &pdev->dev);
    // get resources

    // setup

    // add i2c adapter
    ret = i2c_add_numbered_adapter(&i2c_imx->adapter);

    return 0;
}



/* i2c algorithm */
int i2c_xxx_xfer(struct i2c_adapter* ada, struct i2c_msg* msgs, int num) {
    i2c_xxx_start(i2c_xxx);

    // read/write

    i2c_xxx_stop(i2c_xxx);
}
struct i2c_algorithm i2c_xxx_algo = {
    .master_xfer = i2c_xxx_xfer,
    .functionality = i2c_xxx_func,
};

