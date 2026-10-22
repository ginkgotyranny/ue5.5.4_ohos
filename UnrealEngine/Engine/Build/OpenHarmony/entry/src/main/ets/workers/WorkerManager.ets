/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import worker, { ErrorEvent } from '@ohos.worker';
import {Constants} from '../common/Constants'

export class WorkerManager {
    private static instance: WorkerManager;
    private ueWorker: worker.ThreadWorker;
	private akiWorker: worker.ThreadWorker;
	
    private constructor() {
        this.ueWorker = new worker.ThreadWorker("./UeWorker.ets", {type:"classic", name: "UeWorker"});
		this.akiWorker = new worker.ThreadWorker("./AkiWorker.ets", {type:"classic", name: "AkiWorker"});
        console.log("UE worker created");
		this.ueWorker.onerror = (e : ErrorEvent) => {
			let msg = e.message;
			let filename = e.filename;
			let lineno = e.lineno;
			let colno = e.colno;
            console.error(`on Error ${msg} ${filename} ${lineno} ${colno}`);
        }
    }

    public static getInstance(): WorkerManager {
        if (AppStorage.Get(Constants.APP_KEY_WORKER_MANAGER) == null) {
            AppStorage.SetOrCreate(Constants.APP_KEY_WORKER_MANAGER, new WorkerManager);
        }
        return AppStorage.Get(Constants.APP_KEY_WORKER_MANAGER) as WorkerManager;
    }

    public getWorker(): worker.ThreadWorker {
        return this.ueWorker;
    }
	
	public getAkiWorker():worker.ThreadWorker{
		return this.akiWorker;
	}
}