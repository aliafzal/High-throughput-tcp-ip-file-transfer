/*
 * TCP Veno congestion control
 *
 * This is based on the congestion detection/avoidance scheme described in
 *    C. P. Fu, S. C. Liew.
 *    "TCP Veno: TCP Enhancement for Transmission over Wireless Access Networks."
 *    IEEE Journal on Selected Areas in Communication,
 *    Feb. 2003.
 * 	See http://www.ie.cuhk.edu.hk/fileadmin/staff_upload/soung/Journal/J3.pdf
 */

/*
* TCP AZS (Ali, Zane, Spencer) is an enhanced version of the TCP Veno congestion controller.
*
*/

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>
#include <net/tcp.h>

#define TCP_AZS_INIT_RTT 1000000 /* 1 second */
#define DEFAULT_AZS_WINDOW_SIZE 65000

/* azs variables */
struct azs{
	u8	azs_en;
	u8	if_cong;
	u32		rtt_min;
	u32		rtt;
};

/* initialize azs variables */
static void tcp_azs_init(struct sock *sk)
{
	struct azs *azs = inet_csk_ca(sk);
	
	// initialize AZS variables (RTT and RTT_MIN are initializes to 1 second)
	azs->azs_en = 1;
	azs->if_cong = 0;
	azs->rtt_min = TCP_AZS_INIT_RTT;
	azs->rtt = TCP_AZS_INIT_RTT;
}

/* when packet is acked: 
* because we are choosing a static, large window, we record rtt
* but force a large send window size because we know the network
* is lossy and not congested
*/
static void tcp_azs_pkts_acked(struct sock *sk, u32 cnt, s32 rtt_us)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct azs *azs = inet_csk_ca(sk);

	// if we read a new rtt from the network, put it into the azs rtt
	if (rtt_us > 0)
		azs->rtt = rtt_us;
	
	// store the minimum rtt
	azs->rtt_min = min(azs->rtt_min, azs->rtt);
	
	// force the send window size to our default window size
	tp->snd_cwnd = DEFAULT_AZS_WINDOW_SIZE;
}

/*
* force default window size
*/
static u32 tcp_azs_undo_cwnd(struct sock *sk) {
	return DEFAULT_AZS_WINDOW_SIZE;
}

static void tcp_azs_state(struct sock *sk, u8 ca_state)
{
	struct azs *azs = inet_csk_ca(sk);
	// enable azs when state function is called
	azs->azs_en = 1;
}

/*
* force send window back to default window size upon occurance of any
* collision avoidance event
*/
static void tcp_azs_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	struct tcp_sock *tp = tcp_sk(sk);
	tp->snd_cwnd = DEFAULT_AZS_WINDOW_SIZE;
}

/* instead of relying on veno's dynamic window sizing for congestion avoidance,
* we again force the window to our default window size
*/
static void tcp_azs_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct azs *azs = inet_csk_ca(sk);

	// if azs is not enabled, default to veno
	if (!azs->ezs_en) {
		tcp_veno_cong_avoid(sk, ack, acked);
		return;
	}

	tp->snd_cwnd = DEFAULT_AZS_WINDOW_SIZE;
}

/*
* force slow start threshold to default window size
*/
static u32 tcp_azs_ssthresh(struct sock *sk)
{
	return DEFAULT_AZS_WINDOW_SIZE;
}

static struct tcp_congestion_ops tcp_azs __read_mostly = {
	.init		= tcp_azs_init,
	.ssthresh	= tcp_azs_ssthresh,
	.cong_avoid	= tcp_azs_cong_avoid,
	.cwnd_event = tcp_azs_cwnd_event,
	.pkts_acked	= tcp_azs_pkts_acked,
	.set_state	= tcp_azs_state,
	.undo_cwnd	= tcp_azs_undo_cwnd,

	.owner		= THIS_MODULE,
	.name		= "azs",
};

static int __init tcp_azs_register(void)
{
	BUILD_BUG_ON(sizeof(struct azs) > ICSK_CA_PRIV_SIZE);
	tcp_register_congestion_control(&tcp_azs);
	return 0;
}

static void __exit tcp_azs_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_azs);
}

module_init(tcp_honey_register);
module_exit(tcp_honey_unregister);

MODULE_AUTHOR("Ali Afzal, Zane Pazooki, Spencer McDonough");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP AZS");