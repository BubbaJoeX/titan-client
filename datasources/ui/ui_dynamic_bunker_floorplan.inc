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
		MinimumSize='880,640'
		Name='floorplanWindow'
		PalBgTint='back1'
		ScrollExtent='880,640'
		Size='880,640'
		UserMovable='true'
		UserResizable='false'
	>
		<Data
			Name='CodeData'
			buttonAssign='footer.buttonAssign'
			buttonCancel='footer.buttonCancel'
			buttonClose='header.buttonClose'
			listRooms='content.left.listRooms'
			listSockets='content.left.listSockets'
			textStatus='footer.textStatus'
			textTitle='header.titleText'
			textRoomName='content.right.textRoomName'
			textSocketHint='content.right.textSocketHint'
			viewer='content.right.viewerpage.viewer'
		/>

		<Page BackgroundColor='#FFFFFF' BackgroundOpacity='0.45' BackgroundTint='#00354F' Location='0,0' Name='header' PalBgTint='back1' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='880,36' Size='880,36'>
			<Text Enabled='false' Font='bold_16' LocalText='Bunker Floorplan' Location='14,8' Name='titleText' ScrollExtent='780,22' Size='780,22' TextAlignment='Left' TextColor='#E8F4FF'>Bunker Floorplan</Text>
			<Button LocalText='X' Location='846,6' Name='buttonClose' ScrollExtent='24,24' Size='24,24' Style='/Styles.New.buttons.hud.style'>X</Button>
		</Page>

		<Page GetsInput='true' Location='10,46' Name='content' ScrollExtent='860,528' Size='860,528'>
			<!-- Floorplan room catalog (left) -->
			<Page BackgroundColor='#101820' BackgroundOpacity='0.90' Location='0,0' Name='left' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='280,528' Size='280,528'>
				<Text Enabled='false' Font='bold_12' LocalText='Floorplan' Location='10,8' Name='labelRooms' ScrollExtent='260,18' Size='260,18' TextColor='#7EC8E3'>Floorplan</Text>
				<List
					BackgroundOpacity='0.55'
					DataSource='dsRooms'
					GetsInput='true'
					Location='8,30'
					Name='listRooms'
					RStyleDefault='/Styles.New.whiteOutline.rs_default'
					ScrollExtent='264,300'
					Size='264,300'
					Style='/Styles.list.style'
				/>
				<Text Enabled='false' Font='bold_12' LocalText='Snap Points' Location='10,340' Name='labelSockets' ScrollExtent='260,18' Size='260,18' TextColor='#F0D060'>Snap Points</Text>
				<List
					BackgroundOpacity='0.55'
					DataSource='dsSockets'
					GetsInput='true'
					Location='8,362'
					Name='listSockets'
					RStyleDefault='/Styles.New.whiteOutline.rs_default'
					ScrollExtent='264,154'
					Size='264,154'
					Style='/Styles.list.style'
				/>
				<DataSource Name='dsRooms' />
				<DataSource Name='dsSockets' />
			</Page>

			<!-- Top-down room preview (right) -->
			<Page BackgroundColor='#0C141A' BackgroundOpacity='0.92' Location='290,0' Name='right' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='570,528' Size='570,528'>
				<Text Enabled='false' Font='bold_14' LocalText='Select a room module' Location='12,10' Name='textRoomName' ScrollExtent='546,22' Size='546,22' TextColor='#FFFFFF'>Select a room module</Text>
				<Text Enabled='false' Font='bold_11' LocalText='Yellow snap points are open portals. Black = connected.' Location='12,34' Name='textSocketHint' ScrollExtent='546,18' Size='546,18' TextColor='#C8D0D8'>Yellow snap points are open portals. Black = connected.</Text>
				<Page
					BackgroundTint='#1CFFFF'
					Location='12,60'
					Name='ViewerPage'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline2.rs_default'
					ScrollExtent='546,456'
					Size='546,456'
				>
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
						ScrollExtent='538,448'
						Size='538,448'
					/>
					<Image
						Color='#3A90A8'
						Location='188,120'
						Name='viewerWash'
						Opacity='0.12'
						ScrollExtent='170,170'
						Size='170,170'
						SourceRect='266,0,512,252'
						SourceResource='ui_viewer_back'
					/>
				</Page>
			</Page>
		</Page>

		<Page BackgroundColor='#FFFFFF' BackgroundOpacity='0.35' BackgroundTint='#00354F' Location='0,586' Name='footer' PalBgTint='back1' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='880,54' Size='880,54'>
			<Text Enabled='false' Font='bold_11' LocalText='Select room [Left Mouse]   Rotate preview [Drag]   Assign grafts the room onto the selected snap point.' Location='12,8' Name='textStatus' ScrollExtent='620,38' Size='620,38' TextAlignment='Left' TextColor='#D0E4F0'>Select room [Left Mouse]   Rotate preview [Drag]   Assign grafts the room onto the selected snap point.</Text>
			<Button LocalText='Assign' Location='650,12' Name='buttonAssign' ScrollExtent='100,30' Size='100,30' Style='/Styles.New.buttons.hud.style'>Assign</Button>
			<Button LocalText='Cancel' Location='760,12' Name='buttonCancel' ScrollExtent='100,30' Size='100,30' Style='/Styles.New.buttons.hud.style'>Cancel</Button>
		</Page>
	</Page>
</Page>
