
// don's sleep
// keep atomic
static blk_status_t my_block_request(struct blk_mq_hw_ctx *, const struct blk_mq_queue_data *) {

}

// block dirver behaviors
static struct blk_mq_ops queue_ops {
	.queue_rq = ;  // core option
}



// sector: start
static void transfer(struct sbull_dev* sd, loff_t offset, unsigned int nbytes, char* buffer, int dir) {
    if (offset + nbytes > sd->size) {
        pr_notice("write out of size: (%ld, %ld)\n", offset, nbytes);
        return;
    }
    if (dir == WRITE) {
        memcpy(sd->data + offset, buffer, nbytes);
    } else {
        memcpy(buffer, sd->data + offset, nbytes);
    }
}

static int block_xfer_bio(struct sbull_dev* sd, struct bio* bio) {
    struct bio_vec bvec;
    struct bvec_iter iter;
    char* buffer = NULL;
    sector_t sector = bio->bi_iter.bi_sector;  // first sector
    loff_t offset = sector << SECTOR_SHIFT;  // to bytes

    bio_for_each_segment(bvec, bio, iter) {
        buffer = kmap_atomic(bvec.bv_page) + bvec.bv_offset;
        unsigned int bytes = bvec.bv_len;
        transfer(sd, offset, bytes, buffer, bio_data_dir(bio));
        offset += bytes;
        kunmap_atomic(buffer);
    }

    return 0;
}

static
int block_xfer_request(struct sbull_dev *sd, struct request *req)
{
	struct bio *bio;
	int nsect = 0;

	__rq_for_each_bio(bio, req) {
		block_xfer_bio(sd, bio);
		nsect += bio->bi_iter.bi_size >> SECTOR_SHIFT;
	}
	return 0;
}

static blk_status_t sbull_block_request_full(struct blk_mq_hw_ctx* hctx, 
                                             const struct blk_mq_queue_data* qd) {
    blk_status_t rv = BLK_STS_OK;
    int sectors_xferred;
	struct request *req = qd->rq;
	struct sbull_dev *dev = req->q->queuedata;
	
    blk_mq_start_request(req);  // start handle request
	if (blk_rq_is_passthrough(req)) {
		pr_notice("Skip non-fs request\n");
		rv = BLK_STS_IOERR;
		goto done;
	}
	// do work
	sectors_xferred = block_xfer_request(sd, req);

done:
	blk_mq_end_request(req, rv);  // end handle request

	return rv;
}

static struct blk_mq_ops mq_ops_full = {
	.queue_rq = sbull_block_request_full,  // handle request
};

static
blk_status_t sbull_block_request_simple(struct blk_mq_hw_ctx *hctx,
			       const struct blk_mq_queue_data *qd)
{
	struct request *req = qd->rq;
	struct sbull_dev *dev = req->q->disk->private_data;
	struct bio_vec bvec;
	struct req_iterator iter;
	sector_t pos_sector = blk_rq_pos(req);
	void *buffer;
	blk_status_t ret = BLK_STS_OK;

	blk_mq_start_request(req);

	if (blk_rq_is_passthrough(req)) {
		pr_notice("Skip non-fs request\n");
		ret = BLK_STS_IOERR;
		goto done;
	}

    loff_t offset = pos_sector * SBULL_SECTOR_SIZE;
	rq_for_each_segment(bvec, req, iter) {
		size_t num_sector = blk_rq_cur_sectors(req);
        unsigned int nbytes = num_sector * SBULL_SECTOR_SIZE;
		bool dir = rq_data_dir(req);
		pr_notice("Req dir: %d, sec %lld, nr %ld\n",
			  dir, pos_sector, num_sector);
		buffer = page_address(bvec.bv_page) + bvec.bv_offset;
		transfer(dev, offset, nbytes, buffer, dir);
		offset += nbytes;
	}


done:
	blk_mq_end_request(req, ret);
	return ret;
}

static struct blk_mq_ops mq_ops_simple = {
	.queue_rq = sbull_block_request_simple,
};


static blk_qc_t mk_request(struct request_queue *q, struct bio *bio) {
	int status;
	struct sbull_dev *dev = q->queuedata;

	status = block_xfer_bio(dev, bio);
	bio->bi_status = status;
	bio_endio(bio);

	return BLK_QC_T_NONE;
}

static void timeout_cb(struct timer_list* timer) {
    struct sbull_dev* sd = from_timer(sd, timer, timer);

    spin_lock(&sd->lock);
	pr_warn("timeout!!\n");	
	spin_unlock(&sd->lock);
}
