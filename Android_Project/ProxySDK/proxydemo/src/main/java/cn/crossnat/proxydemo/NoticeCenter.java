package cn.crossnat.proxydemo;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.os.Handler;

public class NoticeCenter {
	public static interface NoticeDelegate {
		/**
		 * 监听者收到特点广播后回调
		 * 
		 * @param noticeName
		 *            广播名
		 * @param obj
		 *            广播参数
		 */
		public void onRecvNotice(String noticeName, Object obj);
	}

	private static NoticeCenter instance = null;
	private Map<String, List<NoticeDelegate>> delegates = new HashMap<String, List<NoticeDelegate>>();

	public static NoticeCenter Instance() {
		if (instance == null) {
			instance = new NoticeCenter();
		}
		return instance;
	}

	/**
	 * 监听者开始监听特点广播
	 * 
	 * @param noticeName
	 *            广播名称
	 * @param delegate
	 *            监听者
	 */
	public void AddDelegate(String noticeName, NoticeDelegate delegate) {
		synchronized (delegates) {
			List<NoticeDelegate> list = delegates.get(noticeName);
			if (list == null) {
				list = new ArrayList<NoticeDelegate>();
				delegates.put(noticeName, list);
			}
			if (!list.contains(delegate)) {
				list.add(delegate);
			}
		}
	}

	/**
	 * 监听者停止监听特点广播
	 * 
	 * @param noticeName
	 *            广播名称
	 * @param delegate
	 *            监听者
	 */
	public void RemoveDelegate(String noticeName, NoticeDelegate delegate) {
		synchronized (delegates) {
			List<NoticeDelegate> list = delegates.get(noticeName);
			if (list == null) {
				return;
			}
			if (list.contains(delegate)) {
				list.remove(delegate);
			}
			if (list.size() == 0) {
				delegates.remove(noticeName);
			}
		}
	}

	/**
	 * 监听者停止监听的所有广播，
	 * 
	 * @param delegate
	 */
	public void RemoveDelegate(NoticeDelegate delegate) {
		synchronized (delegates) {
			Collection<List<NoticeDelegate>> collect= new ArrayList<List<NoticeDelegate>>(delegates.values());
			for (List<NoticeDelegate> list : collect) {
				if (list.contains(delegate)) {
					list.remove(delegate);
				}
				if (list.size() == 0) {
					delegates.values().remove(list);
				}
			}
		}
	}

	Handler handler=new Handler();
	
	/**
	 * 发送特定广播
	 * 
	 * @param noticeName
	 *            广播名
	 * @param obj
	 *            广播参数
	 */
	public void PostNotice(final String noticeName, final Object obj) {
		synchronized (delegates) {
			final List<NoticeDelegate> list = delegates.get(noticeName);
			if (list == null) {
				return;
			}
			handler.post(new Runnable() {
				@Override
				public void run() {
					for (NoticeDelegate delegate : list) {
						delegate.onRecvNotice(noticeName, obj);
					}
				}
			});
		}
	}

}
