<Page
	BackgroundColor='#000000'
	BackgroundOpacity='0.55'
	Name='DynamicBunkerFloorplan'
	PackSize='1,1'
	ScrollExtent='1024,768'
	Size='1024,768'
	Visible='false'
	VisualEditLock='true'
>
	<Page
		BackgroundColor='#1A2228'
		BackgroundOpacity='0.96'
		BackgroundTint='#0A3040'
		Location='72,48'
		MinimumSize='900,660'
		Name='floorplanWindow'
		PalBgTint='back1'
		ScrollExtent='900,660'
		Size='900,660'
		UserMovable='true'
		UserResizable='false'
	>
		<Data
			Name='CodeData'
			ButtonClose='header.buttonClose'
			StickyVisible='true'
			buttonAssign='footer.buttonAssign'
			buttonUnassign='footer.buttonUnassign'
			buttonCancel='footer.buttonCancel'
			buttonClose='header.buttonClose'
			buttonCreatePortal='footer.buttonCreatePortal'
			listRooms='content.tabTarget.pageCatalog.left.listRooms'
			listSockets='content.tabTarget.pageCatalog.left.listSockets'
			textboxFilter='content.tabTarget.pageCatalog.left.textboxFilter'
			textStatus='footer.textStatus'
			textTitle='header.titleText'
			textRoomName='content.tabTarget.pageCatalog.right.textRoomName'
			textSocketHint='content.tabTarget.pageCatalog.right.textSocketHint'
			textLayoutHint='content.tabTarget.pageLayout.textLayoutHint'
			textPlaceHint='content.tabTarget.pagePlace.textPlaceHint'
			textboxPlaceCell='content.tabTarget.pagePlace.textboxPlaceCell'
			buttonPlacePoints='content.tabTarget.pagePlace.buttonPlacePoints'
			placeWall0='content.tabTarget.pagePlace.placeGrid.placeWall0'
			placeWall1='content.tabTarget.pagePlace.placeGrid.placeWall1'
			placeWall2='content.tabTarget.pagePlace.placeGrid.placeWall2'
			placeWall3='content.tabTarget.pagePlace.placeGrid.placeWall3'
			placeWall4='content.tabTarget.pagePlace.placeGrid.placeWall4'
			placeWall5='content.tabTarget.pagePlace.placeGrid.placeWall5'
			placeWall6='content.tabTarget.pagePlace.placeGrid.placeWall6'
			placeWall7='content.tabTarget.pagePlace.placeGrid.placeWall7'
			placeWall8='content.tabTarget.pagePlace.placeGrid.placeWall8'
			layoutMapCanvas='content.tabTarget.pageLayout.layoutMapCanvas'
			layoutMapViewer='content.tabTarget.pageLayout.layoutMapCanvas.layoutMapViewer'
			viewer='content.tabTarget.pageCatalog.right.viewerpage.viewer'
			tabs='content.tabs'
		/>

		<Page BackgroundColor='#FFFFFF' BackgroundOpacity='0.45' BackgroundTint='#00354F' Location='0,0' Name='header' PalBgTint='back1' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='900,36' Size='900,36'>
			<Text Enabled='false' Font='bold_16' LocalText='Bunker Floorplan' Location='14,8' Name='titleText' ScrollExtent='820,22' Size='820,22' TextAlignment='Left' TextColor='#E8F4FF'>Bunker Floorplan</Text>
			<Button LocalText='X' Location='866,6' Name='buttonClose' ScrollExtent='24,24' Size='24,24' Style='/Styles.New.buttons.hud.style'>X</Button>
		</Page>

		<Page GetsInput='true' Location='10,46' Name='content' ScrollExtent='880,548' Size='880,548'>
			<TabbedPane
				ActiveTab='0'
				DataSource='tabData'
				Location='0,0'
				Name='tabs'
				ScrollExtent='880,22'
				Size='880,22'
				Style='/Styles.New.tabs.horizontal_10.style'
				TargetPage='tabTarget'
			/>
			<DataSource Name='tabData'>
				<Data Name='Catalog' Target='tabTarget.pageCatalog' />
				<Data Name='Layout Map' Target='tabTarget.pageLayout' />
				<Data Name='Place Portal' Target='tabTarget.pagePlace' />
			</DataSource>

			<Page BackgroundColor='#101820' BackgroundOpacity='0.90' Location='0,28' Name='tabTarget' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='880,520' Size='880,520'>
				<!-- Catalog tab -->
				<Page GetsInput='true' Location='0,0' Name='pageCatalog' ScrollExtent='880,520' Size='880,520'>
					<Page BackgroundColor='#000000' BackgroundOpacity='0.40' Location='8,8' Name='left' ScrollExtent='270,504' Size='270,504'>
						<Text Enabled='false' Font='bold_12' LocalText='Room Catalog' Location='8,6' ScrollExtent='250,18' Size='250,18' TextColor='#7EC8E3'>Room Catalog</Text>
						<Textbox
							GetsInput='true'
							Location='8,26'
							MaxLength='64'
							Name='textboxFilter'
							RStyleDefault='/Styles.New.whiteOutline.rs_default'
							ScrollExtent='254,22'
							Size='254,22'
							Style='/Styles.textbox.login.Style_12'
							TextColor='#E8F4FF'
						></Textbox>
						<List
							BackgroundColor='#000000'
							BackgroundOpacity='0.55'
							DataSource='dsRooms'
							GetsInput='true'
							Location='8,52'
							Name='listRooms'
							RStyleDefault='/Styles.list.dark.rs_default'
							ScrollExtent='238,250'
							Size='238,250'
							Style='/Styles.list.combo.style'
							TextColor='#E8F4FF'
						/>
						<DataSource Name='dsRooms' />
						<Scrollbar Control='listRooms' Location='246,52' ScrollExtent='12,250' Size='12,250' Style='/Styles.scrollbar.default.vHud4.style' />
						<Text Enabled='false' Font='bold_12' LocalText='Connections' Location='8,312' ScrollExtent='250,18' Size='250,18' TextColor='#F0D060'>Connections</Text>
						<List
							BackgroundColor='#000000'
							BackgroundOpacity='0.55'
							DataSource='dsSockets'
							GetsInput='true'
							Location='8,332'
							Name='listSockets'
							RStyleDefault='/Styles.list.dark.rs_default'
							ScrollExtent='238,160'
							Size='238,160'
							Style='/Styles.list.combo.style'
							TextColor='#F5E6A8'
						/>
						<DataSource Name='dsSockets' />
						<Scrollbar Control='listSockets' Location='246,332' ScrollExtent='12,160' Size='12,160' Style='/Styles.scrollbar.default.vHud4.style' />
					</Page>
					<Page BackgroundColor='#0C141A' BackgroundOpacity='0.92' Location='286,8' Name='right' ScrollExtent='586,504' Size='586,504'>
						<Text Enabled='false' Font='bold_14' LocalText='Select a room module' Location='10,8' Name='textRoomName' ScrollExtent='566,22' Size='566,22' TextColor='#FFFFFF'>Select a room module</Text>
						<Text Enabled='false' Font='bold_11' LocalText='3D preview of selected catalog module.' Location='10,30' Name='textSocketHint' ScrollExtent='566,18' Size='566,18' TextColor='#C8D0D8'>3D preview of selected catalog module.</Text>
						<Page BackgroundTint='#1CFFFF' Location='10,52' Name='viewerpage' PalBgTint='line1' RStyleDefault='/Styles.New.whiteOutline2.rs_default' ScrollExtent='566,440' Size='566,440'>
							<CuiWidget3dObjectListViewer
								BackgroundColor='#142028'
								BackgroundTint='#0A2030'
								CameraPitch='1.05'
								cameralookatcenter='true'
								cameratransformtoobj='true'
								defaultrotatespeed='0.35'
								dragpitchok='false'
								dragyawok='true'
								fieldofview='50'
								FitDistanceFactor='0.85'
								Location='4,4'
								Name='viewer'
								PalBgTint='back1'
								RStyleDefault='/Styles.New.viewer.rs_default'
								ScrollExtent='558,432'
								Size='558,432'
							/>
						</Page>
					</Page>
				</Page>

				<!-- Layout map tab -->
				<Page GetsInput='true' Location='0,0' Name='pageLayout' ScrollExtent='880,520' Size='880,520' Visible='false'>
					<Text Enabled='false' Font='bold_13' LocalText='Top-down layout — click a node to select a portal for assign/unassign.' Location='12,8' Name='textLayoutHint' ScrollExtent='856,20' Size='856,20' TextColor='#E8F4FF'>Top-down layout — click a node to select a portal for assign/unassign.</Text>
					<Text Enabled='false' Font='bold_11' LocalText='Yellow = open   Green = linked   Purple = custom snap' Location='12,30' ScrollExtent='856,16' Size='856,16' TextColor='#C8D0D8'>Yellow = open   Green = linked   Purple = custom snap</Text>
					<Page
						BackgroundColor='#0A1218'
						BackgroundOpacity='0.98'
						Location='12,52'
						Name='layoutMapCanvas'
						RStyleDefault='/Styles.New.whiteOutline.rs_default'
						ScrollExtent='856,456'
						Size='856,456'
					>
						<CuiWidget3dObjectListViewer
							BackgroundColor='#0A1218'
							BackgroundOpacity='0.0'
							BackgroundTint='#0A2030'
							CameraPitch='1.570796'
							cameralookatcenter='true'
							cameratransformtoobj='true'
							defaultrotatespeed='0.0'
							dragpitchok='false'
							dragyawok='false'
							fieldofview='50'
							FitDistanceFactor='1.1'
							GetsInput='false'
							Location='0,0'
							Name='layoutMapViewer'
							PalBgTint='back1'
							RStyleDefault='/Styles.New.viewer.rs_default'
							ScrollExtent='856,456'
							Size='856,456'
							Visible='false'
						/>
					</Page>
				</Page>

				<!-- Place portal tab -->
				<Page GetsInput='true' Location='0,0' Name='pagePlace' ScrollExtent='880,520' Size='880,520' Visible='false'>
					<Text Enabled='false' Font='bold_13' LocalText='Place two corners in-world to define a custom portal doorway (vertical / 45-degree walls).' Location='12,8' Name='textPlaceHint' ScrollExtent='856,20' Size='856,20' TextColor='#E8F4FF'>Place two corners in-world to define a custom portal doorway (vertical / 45-degree walls).</Text>
					<Text Enabled='false' Font='bold_11' LocalText='Current room (updates when you walk into another cell):' Location='12,34' ScrollExtent='400,16' Size='400,16' TextColor='#C8D0D8'>Current room (updates when you walk into another cell):</Text>
					<Textbox
						Enabled='false'
						GetsInput='false'
						Location='12,54'
						MaxLength='4'
						Name='textboxPlaceCell'
						RStyleDefault='/Styles.New.whiteOutline.rs_default'
						ScrollExtent='80,22'
						Size='80,22'
						Style='/Styles.textbox.login.Style_12'
						TextColor='#E8F4FF'
					>--</Textbox>
					<Button LocalText='Place Points' Location='110,50' Name='buttonPlacePoints' ScrollExtent='120,32' Size='120,32' Style='/Styles.New.buttons.hud.style'>Place Points</Button>
					<Text Enabled='false' Font='bold_11' LocalText='Click Place Points, then click two doorway corners in the world (top-left, then bottom-right). Spheres mark each corner. Use Create Snap when both are set.' Location='12,92' ScrollExtent='856,48' Size='856,48' TextColor='#A0B8C8'>Click Place Points, then click two doorway corners in the world (top-left, then bottom-right). Spheres mark each corner. Use Create Snap when both are set.</Text>
					<Page BackgroundColor='#0A1218' BackgroundOpacity='0.95' Location='120,160' Name='placeGrid' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='320,320' Size='320,320' Visible='false'>
						<Button LocalText='NW' Location='20,20' Name='placeWall0' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>NW</Button>
						<Button LocalText='N' Location='130,20' Name='placeWall1' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>N</Button>
						<Button LocalText='NE' Location='240,20' Name='placeWall2' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>NE</Button>
						<Button LocalText='W' Location='20,130' Name='placeWall3' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>W</Button>
						<Button Enabled='false' LocalText='cell' Location='130,130' Name='placeWall4' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>cell</Button>
						<Button LocalText='E' Location='240,130' Name='placeWall5' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>E</Button>
						<Button LocalText='SW' Location='20,240' Name='placeWall6' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>SW</Button>
						<Button LocalText='S' Location='130,240' Name='placeWall7' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>S</Button>
						<Button LocalText='SE' Location='240,240' Name='placeWall8' ScrollExtent='60,60' Size='60,60' Style='/Styles.New.buttons.hud.style'>SE</Button>
					</Page>
				</Page>
			</Page>
		</Page>

		<Page BackgroundColor='#FFFFFF' BackgroundOpacity='0.35' BackgroundTint='#002838' Location='0,604' Name='footer' PalBgTint='back1' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='900,56' Size='900,56'>
			<Text Enabled='false' Font='bold_11' LocalText='Select a module and portal. Linked nodes can be replaced or unassigned.' Location='14,18' Name='textStatus' ScrollExtent='420,22' Size='420,22' TextColor='#D0E8F0'>Select a module and portal. Linked nodes can be replaced or unassigned.</Text>
			<Button LocalText='Unassign' Location='440,12' Name='buttonUnassign' ScrollExtent='90,32' Size='90,32' Style='/Styles.New.buttons.hud.style'>Unassign</Button>
			<Button LocalText='Assign' Location='540,12' Name='buttonAssign' ScrollExtent='90,32' Size='90,32' Style='/Styles.New.buttons.hud.style'>Assign</Button>
			<Button LocalText='Create Snap' Location='640,12' Name='buttonCreatePortal' ScrollExtent='110,32' Size='110,32' Style='/Styles.New.buttons.hud.style'>Create Snap</Button>
			<Button LocalText='Close' Location='760,12' Name='buttonCancel' ScrollExtent='90,32' Size='90,32' Style='/Styles.New.buttons.hud.style'>Close</Button>
		</Page>
	</Page>
</Page>
